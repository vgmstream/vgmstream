#include "coding.h"


/* PS-ADPCM table, defined as rational numbers (as in the spec) */
static const float ps_adpcm_coefs_f[16][2] = {
        { 0.0       ,  0.0       }, //{   0.0        ,   0.0        },
        { 0.9375    ,  0.0       }, //{  60.0 / 64.0 ,   0.0        },
        { 1.796875  , -0.8125    }, //{ 115.0 / 64.0 , -52.0 / 64.0 },
        { 1.53125   , -0.859375  }, //{  98.0 / 64.0 , -55.0 / 64.0 },
        { 1.90625   , -0.9375    }, //{ 122.0 / 64.0 , -60.0 / 64.0 },
        /* extended table used in few PS3 games, found in ELFs */
        { 0.46875   , -0.0       }, //{  30.0 / 64.0 ,  -0.0 / 64.0 },
        { 0.8984375 , -0.40625   }, //{  57.5 / 64.0 , -26.0 / 64.0 },
        { 0.765625  , -0.4296875 }, //{  49.0 / 64.0 , -27.5 / 64.0 },
        { 0.953125  , -0.46875   }, //{  61.0 / 64.0 , -30.0 / 64.0 },
        { 0.234375  , -0.0       }, //{  15.0 / 64.0 ,  -0.0 / 64.0 },
        { 0.44921875, -0.203125  }, //{  28.75/ 64.0 , -13.0 / 64.0 },
        { 0.3828125 , -0.21484375}, //{  24.5 / 64.0 , -13.75/ 64.0 },
        { 0.4765625 , -0.234375  }, //{  30.5 / 64.0 , -15.0 / 64.0 },
        { 0.5       , -0.9375    }, //{  32.0 / 64.0 , -60.0 / 64.0 },
        { 0.234375  , -0.9375    }, //{  15.0 / 64.0 , -60.0 / 64.0 },
        { 0.109375  , -0.9375    }, //{   7.0 / 64.0 , -60.0 / 64.0 },
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
 * consoles/games/libs would vary (PS1 could do it in hardware using BRR/XA's logic, FMOD may
 * depend on platform, PS3 games use floats, etc). There are rounding diffs between implementations.
 */

/* standard PS-ADPCM (float math version) */
void decode_psx(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int is_badflags, int config) {
    uint8_t frame[0x10] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor, flag;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int extended_mode = (config == 1);


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x02) * 2; /* always 28 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    coef_index   = (frame[0] >> 4) & 0xf;
    shift_factor = (frame[0] >> 0) & 0xf;
    flag = frame[1]; /* only lower nibble needed */

    /* upper filters only used in few PS3 games, normally 0 */
    if (!extended_mode) {
        VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM: incorrect coefs/shift at %x\n", (uint32_t)frame_offset);
        if (coef_index > 5)
            coef_index = 0;
        if (shift_factor > 12)
            shift_factor = 9; /* supposedly, from Nocash PSX docs */
    }

    if (is_badflags) /* some games store garbage or extra internal logic in the flags, must be ignored */
        flag = 0;
    VGM_ASSERT_ONCE(flag > 7,"PS-ADPCM: unknown flag at %x\n", (uint32_t)frame_offset); /* meta should use PSX-badflags */


    shift_factor = 20 - shift_factor;
    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample = 0;

        if (flag < 0x07) { /* with flag 0x07 decoded sample must be 0 */
            uint8_t nibbles = frame[0x02 + i/2];
            
            sample = (i&1 ? /* low nibble first */
                    get_high_nibble_signed(nibbles):
                    get_low_nibble_signed(nibbles)) << shift_factor; /*scale*/
            sample = sample + (int32_t)((ps_adpcm_coefs_f[coef_index][0]*hist1 + ps_adpcm_coefs_f[coef_index][1]*hist2) * 256.0f);
            sample >>= 8;
        }

        outbuf[sample_count] = clamp16(sample); /*clamping*/
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}


/* PS-ADPCM with configurable frame size and no flag (int math version).
 * Found in some PC/PS3 games (FF XI in sizes 0x3/0x5/0x9/0x41, Afrika in size 0x4, Blur/James Bond in size 0x33, etc).
 *
 * Uses int/float math depending on config (PC/other code may be int, PS3 float). */
void decode_psx_configurable(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size, int config) {
    uint8_t frame[0x50] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int extended_mode = (config == 1);
    int float_mode = (config == 1);


    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    coef_index   = (frame[0] >> 4) & 0xf;
    shift_factor = (frame[0] >> 0) & 0xf;

    /* upper filters only used in few PS3 games, normally 0 */
    if (!extended_mode) {
        VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM: incorrect coefs/shift at %x\n", (uint32_t)frame_offset);
        if (coef_index > 5)
            coef_index = 0;
        if (shift_factor > 12)
            shift_factor = 9; /* supposedly, from Nocash PSX docs */
    }


    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample = 0;
        uint8_t nibbles = frame[0x01 + i/2];

        sample = i&1 ? /* low nibble first */
                (nibbles >> 4) & 0x0f :
                (nibbles >> 0) & 0x0f;
        sample = (int16_t)((sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
        sample = float_mode ?
            (int32_t)(sample + ps_adpcm_coefs_f[coef_index][0]*hist1 + ps_adpcm_coefs_f[coef_index][1]*hist2) :
            sample + ((ps_adpcm_coefs_i[coef_index][0]*hist1 + ps_adpcm_coefs_i[coef_index][1]*hist2) >> 6);
        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

/* PS-ADPCM from Pivotal games, exactly like psx_cfg but with float math (reverse engineered from the exe) */
void decode_psx_pivotal(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size) {
    uint8_t frame[0x50] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;


    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    coef_index   = (frame[0] >> 4) & 0xf;
    shift_factor = (frame[0] >> 0) & 0xf;

    VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM-piv: incorrect coefs/shift\n");
    if (coef_index > 5) /* just in case */
        coef_index = 5;
    if (shift_factor > 12) /* same */
        shift_factor = 12;

    shift_factor = 20 - shift_factor;
    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample = 0;
        uint8_t nibbles = frame[0x01 + i/2];

        sample = (i&1 ? /* low nibble first */
                get_high_nibble_signed(nibbles):
                get_low_nibble_signed(nibbles)) << shift_factor; /*scale*/
        sample = sample + (int32_t)((ps_adpcm_coefs_f[coef_index][0]*hist1 + ps_adpcm_coefs_f[coef_index][1]*hist2) * 256.0f); /* actually substracts negative coefs but whatevs */
        sample >>= 8;

        outbuf[sample_count] = clamp16(sample); /*clamping*/
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample; /* not clamped but no difference */
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}


/* Find loop samples in PS-ADPCM data and return if the file loops.
 *
 * PS-ADPCM/VAG has optional bit flags that control looping in the SPU.
 * Possible combinations (as usually defined in Sony's docs):
 * - 0x0 (0000): Normal decode
 * - 0x1 (0001): End marker (last frame)
 * - 0x2 (0010): Loop region (marks files that *may* have loop flags somewhere)
 * - 0x3 (0011): Loop end (jump to loop address)
 * - 0x4 (0100): Start marker
 * - 0x5 (0101): Same as 0x07? Extremely rare [Blood Omen: Legacy of Kain (PS1)]
 * - 0x6 (0110): Loop start (save loop address)
 * - 0x7 (0111): End marker and don't decode
 * - 0x8+(1NNN): Not valid
 */
static int ps_find_loop_offsets_internal(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t * p_loop_start, int32_t * p_loop_end, int config) {
    int num_samples = 0, loop_start = 0, loop_end = 0;
    int loop_start_found = 0, loop_end_found = 0;
    off_t offset = start_offset;
    off_t max_offset = start_offset + data_size;
    size_t interleave_consumed = 0;
    int detect_full_loops = config & 1;


    if (data_size == 0 || channels == 0 || (channels > 1 && interleave == 0))
        return 0;

    while (offset < max_offset) {
        uint8_t flag = read_u8(offset+0x01, sf) & 0x0F; /* lower nibble only (for HEVAG) */

        /* theoretically possible and would use last 0x06 */
        VGM_ASSERT_ONCE(loop_start_found && flag == 0x06, "PS LOOPS: multiple loop start found at %x\n", (uint32_t)offset);

        if (flag == 0x06 && !loop_start_found) {
            loop_start = num_samples; /* loop start before this frame */
            loop_start_found = 1;
        }

        if (flag == 0x03 && !loop_end) {
            loop_end = num_samples + 28; /* loop end after this frame */
            loop_end_found = 1;

            /* ignore strange case in Commandos (PS2), has many loop starts and ends */
            if (channels == 1
                    && offset + 0x10 < max_offset
                    && (read_u8(offset + 0x11, sf) & 0x0F) == 0x06) {
                loop_end = 0;
                loop_end_found = 0;
            }

            if (loop_start_found && loop_end_found)
               break;
        }

        /* hack for some games that don't have loop points but do full loops,
         * if there is a "partial" 0x07 end flag pretend it wants to loop
         * (sometimes this will loop non-looping tracks, and won't loop all repeating files)
         * seems only used in Ratchet & Clank series and Ecco the Dolphin */
        if (flag == 0x01 && detect_full_loops) {
            static const uint8_t eof[0x10] = {0xFF,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
            uint8_t buf[0x10];
            uint8_t hdr = read_u8(offset + 0x00, sf);

            int read = read_streamfile(buf, offset+0x10, sizeof(buf), sf);
            if (read > 0
                    && buf[0] != 0x00 /* ignore blank frame */
                    && buf[0] != 0x0c /* ignore silent frame */
                    && buf[0] != 0x3c /* ignore some L-R tracks with different end flags */
                    && buf[0] != 0x1c /* ignore some L-R tracks with different end flags */
                    ) {

                /* assume full loop with repeated frame header and null frame */
                if (hdr == buf[0] && memcmp(buf+1, eof+1, sizeof(buf) - 1) == 0) {
                    loop_start = 28; /* skip first frame as it's null in PS-ADPCM */
                    loop_end = num_samples + 28; /* loop end after this frame */
                    loop_start_found = 1;
                    loop_end_found = 1;
                    //;VGM_LOG("PS LOOPS: full loop found\n");
                    break;
                }
            }
        }


        num_samples += 28;
        offset += 0x10;

        /* skip other channels */
        interleave_consumed += 0x10;
        if (interleave_consumed == interleave) {
            interleave_consumed = 0;
            offset += interleave*(channels - 1);
        }
    }

    VGM_ASSERT(loop_start_found && !loop_end_found, "PS LOOPS: found loop start but not loop end\n");
    VGM_ASSERT(loop_end_found && !loop_start_found, "PS LOOPS: found loop end but not loop start\n");
    //;VGM_LOG("PS LOOPS: start=%i, end=%i\n", loop_start, loop_end);

    /* From Sony's docs: if only loop_end is set loop back to "phoneme region start", but in practice doesn't */
    if (loop_start_found && loop_end_found) {
        *p_loop_start = loop_start;
        *p_loop_end = loop_end;
        return 1;
    }

    return 0; /* no loop */
}

int ps_find_loop_offsets(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t* p_loop_start, int32_t* p_loop_end) {
    return ps_find_loop_offsets_internal(sf, start_offset, data_size, channels, interleave, p_loop_start, p_loop_end, 0);
}

int ps_find_loop_offsets_full(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t* p_loop_start, int32_t* p_loop_end) {
    return ps_find_loop_offsets_internal(sf, start_offset, data_size, channels, interleave, p_loop_start, p_loop_end, 1);
}

size_t ps_find_padding(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int discard_empty) {
    off_t min_offset, offset, read_offset = 0;
    size_t frame_size = 0x10;
    size_t padding_size = 0;
    size_t interleave_consumed = 0;
    uint8_t buf[0x8000];
    int buf_pos = 0;
    int bytes;


    if (data_size == 0 || channels == 0 || (channels > 1 && interleave == 0))
        return 0;

    offset = start_offset + data_size;

    /* in rare cases (ex. Gitaroo Man) channels have inconsistent empty padding, use first as guide */
    offset = offset - interleave * (channels - 1);

    /* some files have padding spanning multiple interleave blocks */
    min_offset = start_offset; //offset - interleave;

    while (offset > min_offset) {
        uint32_t f1,f2,f3,f4;
        uint8_t flag;
        int is_empty = 0;

        /* read in chunks to optimize (less SF rebuffering since we go in reverse) */
        if (offset < read_offset || buf_pos <= 0) {
            read_offset = offset - sizeof(buf);
            if (read_offset < 0)
                read_offset = 0; //?
            bytes = read_streamfile(buf, read_offset, sizeof(buf), sf);
            buf_pos = (bytes / frame_size * frame_size);
        }

        buf_pos -= frame_size;
        offset -= frame_size;

        f1 = get_u32be(buf+buf_pos+0x00);
        f2 = get_u32be(buf+buf_pos+0x04);
        f3 = get_u32be(buf+buf_pos+0x08);
        f4 = get_u32be(buf+buf_pos+0x0c);
        flag = (f1 >> 16) & 0xFF;

        if (f1 == 0 && f2 == 0 && f3 == 0 && f4 == 0)
            is_empty = 1;

        if (!is_empty && discard_empty) {
            if (flag == 0x07 || flag == 0x77)
                is_empty = 1; /* 'discard frame' flag */
            else if ((f1 & 0xFF00FFFF) == 0 && f2 == 0 && f3 == 0 && f4 == 0)
                is_empty = 1; /* silent with flags (typical for looping files) */
            else if ((f1 & 0xFF00FFFF) == 0x0C000000 && f2 == 0 && f3 == 0 && f4 == 0)
                is_empty = 1; /* silent (maybe shouldn't ignore flag 0x03?) */
            else if ((f1 & 0x0000FFFF) == 0x00007777 && f2 == 0x77777777 && f3 ==0x77777777 && f4 == 0x77777777)
                is_empty = 1; /* silent-ish */
        }

        if (!is_empty)
            break;

        padding_size += frame_size * channels;

        /* skip other channels */
        interleave_consumed += 0x10;
        if (interleave_consumed == interleave) {
            interleave_consumed = 0;
            offset -= interleave * (channels - 1);
            buf_pos -= interleave * (channels - 1);
        }
    }

    //;VGM_LOG("PSX PAD: total size %x\n", padding_size);
    return padding_size;
}


size_t ps_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 0x10 * 28;
}

size_t ps_cfg_bytes_to_samples(size_t bytes, size_t frame_size, int channels) {
    int samples_per_frame = (frame_size - 0x01) * 2;
    return bytes / channels / frame_size * samples_per_frame;
}

/* test PS-ADPCM frames for correctness */
int ps_check_format(STREAMFILE* sf, off_t offset, size_t max) {
    off_t max_offset = offset + max;
    if (max_offset > get_streamfile_size(sf))
        max_offset = get_streamfile_size(sf);

    while (offset < max_offset) {
        uint8_t predictor = (read_8bit(offset+0x00,sf) >> 4) & 0x0f;
        uint8_t flags     =  read_8bit(offset+0x01,sf);

        if (predictor > 5 || flags > 7) {
            return 0;
        }
        offset += 0x10;
    }

    return 1;
}
