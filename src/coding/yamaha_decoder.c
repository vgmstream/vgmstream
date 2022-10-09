#include "../util.h"
#include "coding.h"

/* fixed point amount to scale the current step size */
static const unsigned int scale_step_aica[16] = {
    230, 230, 230, 230, 307, 409, 512, 614,
    230, 230, 230, 230, 307, 409, 512, 614
};

static const int scale_step_adpcmb[16] = {
    57, 57, 57, 57, 77, 102, 128, 153,
    57, 57, 57, 57, 77, 102, 128, 153,
};

static const int scale_step_capcom[8] = {
    58982, 58982, 58982, 58982, 78643, 104858, 131072, 157286,
};

/* look-up for 'mul' IMA's sign*((code&7) * 2 + 1) for every code */
static const int scale_delta[16] = {
      1,  3,  5,  7,  9, 11, 13, 15,
     -1, -3, -5, -7, -9,-11,-13,-15
};

/* Yamaha ADPCM-B (aka DELTA-T) expand used in YM2608/YM2610/etc (cross referenced with various sources and .so) */
static void yamaha_adpcmb_expand_nibble(uint8_t byte, int shift, int32_t* hist1, int32_t* step_size, int16_t *out_sample) {
    int code, delta, sample;

    code =  (byte >> shift) & 0xf;
    delta = ((((code & 0x7) * 2) + 1) * (*step_size)) >> 3; /* like 'mul' IMA */
    if (code & 8)
        delta = -delta;
    sample = *hist1 + delta;

    sample = clamp16(sample); /* not needed in Aska but seems others do */

    *step_size = ((*step_size) * scale_step_adpcmb[code]) >> 6;
    if (*step_size < 0x7f) *step_size = 0x7f;
    else if (*step_size > 0x6000) *step_size = 0x6000;

    *out_sample = sample;
    *hist1 = sample;
}

/* Yamaha AICA expand, slightly filtered vs "ACM" Yamaha ADPCM, same as Creative ADPCM
 * (some info from https://github.com/vgmrips/vgmplay, https://wiki.multimedia.cx/index.php/Creative_ADPCM) */
static void yamaha_aica_expand_nibble(uint8_t byte, int shift, int32_t* hist1, int32_t* step_size, int16_t *out_sample) {
    int code, delta, sample;

    *hist1 = *hist1 * 254 / 256; /* hist filter is vital to get correct waveform but not done in many emus */

    code = (byte >> shift) & 0xf;
    delta = (*step_size * scale_delta[code]) / 8; /* 'mul' IMA with table (not sure if part of encoder) */
    sample = *hist1 + delta;

    sample = clamp16(sample); /* apparently done by official encoder */

    *step_size = ((*step_size) * scale_step_aica[code]) >> 8;
    if (*step_size < 0x7f) *step_size = 0x7f;
    else if (*step_size > 0x6000) *step_size = 0x6000;

    *out_sample = sample;
    *hist1 = sample;
}

/* Capcom's version of Yamaha expand */
static void yamaha_capcom_expand_nibble(uint8_t byte, int shift, int32_t* hist1, int32_t* step_size, int16_t *out_sample) {
    int code, ucode, delta, sample;
    const int scale = 0x200; /* ADPCM state var, but seemingly fixed */

    code =  (byte >> shift) & 0xf;
    ucode = code & 0x7;
    delta = (ucode * (*step_size)) >> 2; /* custom (SH2 CPU can't do odd shifts in one op, it seems) */
    if (code & 8)
        delta = -delta;
    sample = *hist1 + delta;

    sample = (short)sample; /* clamp not done, but output is always low-ish */

    *step_size = ((*step_size) * scale_step_capcom[ucode]) >> 16;
    if (*step_size < 0x80) *step_size = 0x80; /* unlike usual 0x7f */
    else if (*step_size > 0x6000) *step_size = 0x6000;

    *hist1 = sample;

    /* OG code adds out sample, but seems to be always 0 (used for mono downmix?) */
    sample = ((scale * sample) >> 8) /*+ *out_sample */;
    *out_sample = sample;
}


/* info about Yamaha ADPCM as created by official yadpcm.acm (in 'Yamaha-ADPCM-ACM-Driver-100-j')
 * - possibly RIFF codec 0x20
 * - simply called "Yamaha ADPCM Codec" (even though not quite like Yamaha ADPCM-B)
 * - block_align = (sample_rate / 0x3C + 0x04) * channels (ex. 0x2E6 for 22050 stereo, probably given in RIFF)
 * - low nibble first, stereo or mono modes (no interleave)
 * - expand (old IMA 'shift+add' style, not 'mul' style):
 *      delta = step_size >> 3;
 *      if (code & 1) delta += step_size >> 2;
 *      if (code & 2) delta += step_size >> 1;
 *      if (code & 4) delta += step_size;
 *      if (code & 8) delta = -delta;
 *      sample = hist + clamp16(delta);
 *   though compiled more like:
 *      sample = hist + (1-2*(code & 8) * (step_size/8 + step_size/2 * (code&2) + step_size/4 * (code&1) + step_size * (code&4))
 * - step_size update:
 *      step_size = clamp_range(step_size * scale_step_aica[code] >> 8, 0x7F, 0x6000)
 */


/* Yamaha AICA ADPCM (also used in YMZ263B/YMZ280B with high nibble first) */
void decode_aica(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo, int is_high_first) {
    int i, sample_count = 0;
    int16_t out_sample;
    int32_t hist1 = stream->adpcm_history1_16;
    int step_size = stream->adpcm_step_index;

    /* no header (external setup), pre-clamp for wrong values */
    if (step_size < 0x7f) step_size = 0x7f;
    if (step_size > 0x6000) step_size = 0x6000;

    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t byte;
        off_t offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        int shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high/L, odd = low/R */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low/L, odd = high/L */

        byte = read_u8(offset, stream->streamfile);
        yamaha_aica_expand_nibble(byte, shift, &hist1, &step_size, &out_sample);
        outbuf[sample_count] = out_sample;
        sample_count += channelspacing;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_size;
}

/* Capcom/Saturn Yamaha ADPCM, reverse engineered from the exe (codec has no apparent name so CP_YM = Capcom Yamaha) */
void decode_cp_ym(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo) {
    int i, sample_count = 0;
    int16_t out_sample;
    int32_t hist1 = stream->adpcm_history1_16;
    int step_size = stream->adpcm_step_index;

    /* no header (external setup), pre-clamp for wrong values */
    if (step_size < 0x80) step_size = 0x80;
    if (step_size > 0x6000) step_size = 0x6000;

    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t byte;
        uint32_t offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        int shift = is_stereo ?
                (!(channel&1) ? 0:4) :  /* even = low/L, odd = high/R */
                (!(i&1) ? 0:4);         /* low nibble first */

        byte = read_u8(offset, stream->streamfile);
        yamaha_capcom_expand_nibble(byte, shift, &hist1, &step_size, &out_sample);
        outbuf[sample_count] = out_sample;
        sample_count += channelspacing;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_size;
}


/* tri-Ace Aska ADPCM, Yamaha ADPCM-B with headered frames (reversed from Android SO's .so)
 * implements table with if-else/switchs too but that's too goofy */
void decode_aska(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, size_t frame_size) {
    uint8_t frame[0x100] = {0}; /* known max is 0xC0 */
    off_t frame_offset;
    int i, sample_count = 0, frames_in;
    int16_t out_sample;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_size = stream->adpcm_step_index;

    /* external interleave */
    int block_samples = (frame_size - 0x04*channelspacing) * 2 / channelspacing;
    frames_in = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    if (frame_size > sizeof(frame)) {
        VGM_LOG_ONCE("ASKA: unknown frame size %x\n", frame_size);
        return;
    }

    /* parse frame */
    frame_offset = stream->offset + frame_size * frames_in;
    read_streamfile(frame, frame_offset, frame_size, stream->streamfile); /* ignore EOF errors */

    /* header (hist+step) */
    if (first_sample == 0) {
        hist1     = get_s16le(frame + 0x04*channel + 0x00);
        step_size = get_s16le(frame + 0x04*channel + 0x02);
        /* in most files 1st frame has step 0 but it seems ok and needed for correct waveform */
        //if (step_size < 0x7f) step_size = 0x7f;
        //else if (step_size > 0x6000) step_size = 0x6000;
    }

    /* decode nibbles (layout: one nibble per channel, low-high order, ex 6ch=10325410 32541032 ...) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int pos = (channelspacing == 1) ?
                (0x04*channelspacing) + i/2 :
                (0x04*channelspacing) + (i * 4 * channelspacing + 4*channel) / 8; /* nibble position to closest byte */
        int shift = (channelspacing == 1) ? /* low first */
                (!(i&1) ? 0:4) :
                (!(channel&1) ? 0:4);

        yamaha_adpcmb_expand_nibble(frame[pos], shift, &hist1, &step_size, &out_sample);
        outbuf[sample_count] = out_sample;
        sample_count += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_size;
}


/* NXAP ADPCM, Yamaha ADPCM-B with weird headered frames, partially rev'd from the ELF */
void decode_nxap(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x40] = {0}; /* known max is 0xC0 */
    off_t frame_offset;
    int i, sample_count = 0, frames_in;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_size = stream->adpcm_step_index;
    int16_t out_sample;

    /* external interleave, mono */
    size_t frame_size = 0x40;
    int block_samples = (frame_size - 0x4) * 2;
    frames_in = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    /* parse frame */
    frame_offset = stream->offset + frame_size * frames_in;
    read_streamfile(frame, frame_offset, frame_size, stream->streamfile); /* ignore EOF errors */

    /* header (hist+step) */
    if (first_sample == 0) {
        hist1     = get_s16le(frame + 0x00);
        step_size = get_u16le(frame + 0x02) >> 1; /* remove lower bit, also note unsignedness */
        if (step_size < 0x7f) step_size = 0x7f;
        else if (step_size > 0x6000) step_size = 0x6000;
        /* step's lower bit is hist1 sign (useless), and code doesn't seem to do anything useful with it? */
    }

    /* decode nibbles (layout: all nibbles from one channel) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int pos = 0x04 + i/2;
        int shift = (i&1?0:4);

        yamaha_adpcmb_expand_nibble(frame[pos], shift, &hist1, &step_size, &out_sample);
        outbuf[sample_count] = out_sample;
        sample_count += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_size;
}

size_t yamaha_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    /* 2 samples per byte (2 nibbles) in stereo or mono config */
    return bytes * 2 / channels;
}

size_t aska_bytes_to_samples(size_t bytes, size_t frame_size, int channels) {
    int block_align = frame_size;
    if (channels <= 0) return 0;
    return (bytes / block_align) * (block_align - 0x04*channels) * 2 / channels
            + ((bytes % block_align) ? ((bytes % block_align) - 0x04*channels) * 2 / channels : 0);
}
