#include "coding.h"


/* PS-ADPCM table, defined as rational numbers (as in the spec) */
static const double ps_adpcm_coefs_f[5][2] = {
        {   0.0        ,   0.0        },
        {  60.0 / 64.0 ,   0.0        },
        { 115.0 / 64.0 , -52.0 / 64.0 },
        {  98.0 / 64.0 , -55.0 / 64.0 },
        { 122.0 / 64.0 , -60.0 / 64.0 },
};

/* PS-ADPCM table, defined as spec_coef*64 (for int implementations) */
static const int ps_adpcm_coefs_i[5][2] = {
        {   0 ,   0 },
        {  60 ,   0 },
        { 115 , -52 },
        {  98 , -55 },
        { 122 , -60 },
#if 0
        /* extended table from PPSSPP (PSP emu), found by tests (unused?) */
        {   0 ,   0 },
        {   0 ,   0 },
        {  52 ,   0 },
        {  55 ,  -2 },
        {  60 ,-125 },
        {   0 ,   0 },
        {   0 , -91 },
        {   0 ,   0 },
        {   2 ,-216 },
        { 125 ,  -6 },
        {   0 ,-151 },
#endif
};


/* Decodes Sony's PS-ADPCM (sometimes called SPU-ADPCM or VAG, just "ADPCM" in the SDK docs).
 * Very similar to XA ADPCM (see xa_decoder for extended info).
 *
 * Some official PC tools decode using float coefs (from the spec), as does this code, but
 * consoles/games/libs would vary (PS1 could do it in hardware using BRR/XA's logic, FMOD/PS3
 * may use int math in software, etc). There are inaudible rounding diffs between implementations.
 *
 * Optional bit flag combinations in the header control the SPU:
 *  0x0 (0000): Nothing
 *  0x1 (0001): End marker + decode
 *  0x2 (0010): Loop region
 *  0x3 (0011): Loop end
 *  0x4 (0100): Start marker
 *  0x6 (0110): Loop start
 *  0x7 (0111): End marker + don't decode
 *  0x5/8+ (1NNN): Not valid
 */

/* standard PS-ADPCM (float math version) */
void decode_psx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int is_badflags) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor, flag;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x02) * 2; /* always 28 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    coef_index   = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 4) & 0xf;
    shift_factor = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 0) & 0xf;
    flag = (uint8_t)read_8bit(frame_offset+0x01,stream->streamfile); /* only lower nibble needed */

    VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM: incorrect coefs/shift at %lx\n", frame_offset);
    if (coef_index > 5) /* needed by inFamous (PS3) (maybe it's supposed to use more filters?) */
        coef_index = 0; /* upper filters aren't used in PS1/PS2, maybe in PSP/PS3? */
    if (shift_factor > 12)
        shift_factor = 9; /* supposedly, from Nocash PSX docs */

    if (is_badflags) /* some games store garbage or extra internal logic in the flags, must be ignored */
        flag = 0;
    VGM_ASSERT_ONCE(flag > 7,"PS-ADPCM: unknown flag at %lx\n", frame_offset); /* meta should set PSX-badflags */

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample = 0;

        if (flag < 0x07) { /* with flag 0x07 decoded sample must be 0 */
            uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x02+i/2,stream->streamfile);

            new_sample = i&1 ? /* low nibble first */
                    (nibbles >> 4) & 0x0f :
                    (nibbles >> 0) & 0x0f;
            new_sample = (int16_t)((new_sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
            new_sample = (int)(new_sample + ps_adpcm_coefs_f[coef_index][0]*hist1 + ps_adpcm_coefs_f[coef_index][1]*hist2);
            new_sample = clamp16(new_sample);
        }

        outbuf[sample_count] = new_sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = new_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}


/* PS-ADPCM with configurable frame size and no flag (int math version).
 * Found in some PC/PS3 games (FF XI in sizes 3/5/9/41, Afrika in size 4, Blur/James Bond in size 33, etc).
 *
 * Uses int math to decode, which seems more likely (based on FF XI PC's code in Moogle Toolbox). */
void decode_psx_configurable(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* always 28 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    coef_index   = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 4) & 0xf;
    shift_factor = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 0) & 0xf;

    VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM: incorrect coefs/shift at %lx\n", frame_offset);
    if (coef_index > 5) /* needed by Afrika (PS3) (maybe it's supposed to use more filters?) */
        coef_index = 0; /* upper filters aren't used in PS1/PS2, maybe in PSP/PS3? */
    if (shift_factor > 12)
        shift_factor = 9; /* supposedly, from Nocash PSX docs */

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample = 0;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x01+i/2,stream->streamfile);

        new_sample = i&1 ? /* low nibble first */
                (nibbles >> 4) & 0x0f :
                (nibbles >> 0) & 0x0f;
        new_sample = (int16_t)((new_sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
        new_sample = new_sample + ((ps_adpcm_coefs_i[coef_index][0]*hist1 + ps_adpcm_coefs_i[coef_index][1]*hist2) >> 6);
        new_sample = clamp16(new_sample);

        outbuf[sample_count] = new_sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = new_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

size_t ps_bytes_to_samples(size_t bytes, int channels) {
    return bytes / channels / 0x10 * 28;
}

size_t ps_cfg_bytes_to_samples(size_t bytes, size_t frame_size, int channels) {
    return bytes / channels / frame_size * 28;
}
