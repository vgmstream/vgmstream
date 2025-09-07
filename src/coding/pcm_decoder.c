#include "coding.h"
#include "../util.h"
#include <math.h>
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "../util/endianness.h"

void decode_pcm16le(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_16bitLE(stream->offset+i*2,stream->streamfile);
    }
}

void decode_pcm16be(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_16bitBE(stream->offset+i*2,stream->streamfile);
    }
}

void decode_pcm16_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian) {
    int i, sample_count;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = big_endian ? read_16bitBE : read_16bitLE;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_16bit(stream->offset+i*2*channelspacing,stream->streamfile);
    }
}

void decode_pcm8(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_8bit(stream->offset+i,stream->streamfile)*0x100;
    }
}

void decode_pcm8_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_8bit(stream->offset+i*channelspacing,stream->streamfile)*0x100;
    }
}

void decode_pcm8_unsigned(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int16_t v = (uint8_t)read_8bit(stream->offset+i,stream->streamfile);
        outbuf[sample_count] = v*0x100 - 0x8000;
    }
}

void decode_pcm8_unsigned_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int16_t v = (uint8_t)read_8bit(stream->offset+i*channelspacing,stream->streamfile);
        outbuf[sample_count] = v*0x100 - 0x8000;
    }
}

void decode_pcm8_sb(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int16_t v = (uint8_t)read_8bit(stream->offset+i,stream->streamfile);
        if (v&0x80) v = 0-(v&0x7f);
        outbuf[sample_count] = v*0x100;
    }
}

void decode_pcm4(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, nibble_shift, is_high_first, is_stereo;
    int32_t sample_count;
    int16_t v;
    off_t byte_offset;

    is_high_first = (vgmstream->codec_config & 1);
    is_stereo = (vgmstream->channels != 1);

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel (assumed, not sure if stereo version actually exists) */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        nibble_shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high, odd = low */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low, odd = high */

        v = (int16_t)read_8bit(byte_offset, stream->streamfile);
        v = (v >> nibble_shift) & 0x0F;
        outbuf[sample_count] = v*0x11*0x100;
    }
}

void decode_pcm4_unsigned(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, nibble_shift, is_high_first, is_stereo;
    int32_t sample_count;
    int16_t v;
    off_t byte_offset;

    is_high_first = (vgmstream->codec_config & 1);
    is_stereo = (vgmstream->channels != 1);

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel (assumed, not sure if stereo version actually exists) */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        nibble_shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high, odd = low */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low, odd = high */

        v = (int16_t)read_8bit(byte_offset, stream->streamfile);
        v = (v >> nibble_shift) & 0x0F;
        outbuf[sample_count] = v*0x11*0x100 - 0x8000;
    }
}


// TODO: remove after public API is used
static void decode_pcmfloat_i16(VGMSTREAMCHANNEL* stream, int16_t* outbuf, int channels, int first_sample, int samples_to_do, bool big_endian) {
    read_f32_t read_f32 = big_endian ? read_f32be : read_f32le;
    int s = 0;
    off_t offset = stream->offset + first_sample * 0x04;
    while (s < samples_to_do) {
        float sample_float = read_f32(offset, stream->streamfile);
        int sample_pcm = (int)floor(sample_float * 32767.f + .5f);
        outbuf[s] = clamp16(sample_pcm);
        s += channels;
        offset += 0x04;
    }
}

static void decode_pcmfloat(VGMSTREAMCHANNEL* stream, float* buf, int channels, int first_sample, int samples_to_do, bool big_endian) {
    read_f32_t read_f32 = big_endian ? read_f32be : read_f32le;
    int s = 0;
    off_t offset = stream->offset + first_sample * 0x04;
    while (s < samples_to_do) {
        buf[s] = read_f32(offset, stream->streamfile);
        s += channels;
        offset += 0x04;
    }
}

static bool decode_buf_pcmfloat(VGMSTREAM* v, sbuf_t* sdst) {
    decode_state_t* ds = v->decode_state;
    bool big_endian = v->codec_endian;

    if (sdst->fmt == SFMT_S16) {
        //TODO remove
        // using vgmstream without API (render_vgmstream) usually passes a S16 buf
        // could handle externally but blah blah, allow as-is for now
        int16_t* buffer = sdst->buf;
        buffer += sdst->filled * v->channels;
        for (int ch = 0; ch < v->channels; ch++) {
            decode_pcmfloat_i16(&v->ch[ch], buffer + ch, v->channels, ds->samples_into, ds->samples_left, big_endian);
        }
    }
    else {
        float* buffer = sdst->buf;
        buffer += sdst->filled * v->channels;
        for (int ch = 0; ch < v->channels; ch++) {
            decode_pcmfloat(&v->ch[ch], buffer + ch, v->channels, ds->samples_into, ds->samples_left, big_endian);
        }
    }

    return true;
}


static inline int32_t read_s24be(off_t offset, STREAMFILE* sf) {
    return (read_s16be(offset + 0x00, sf) << 8) | read_u8(offset + 0x02, sf);
}

static inline int32_t read_s24le(off_t offset, STREAMFILE* sf) {
    return read_u8(offset + 0x00, sf) | (read_s16le(offset + 0x01, sf) << 8);
}

// TODO: remove after public API is used
static void decode_pcm24_i16(VGMSTREAMCHANNEL* stream, int16_t* buf, int channels, int first_sample, int samples_to_do, bool big_endian) {
    read_s32_t read_s24 = big_endian ? read_s24be : read_s24le;
    int s = 0;
    off_t offset = stream->offset + first_sample * 0x03;
    while (s < samples_to_do) {
        buf[s] = read_s24(offset, stream->streamfile) >> 8;
        s += channels;
        offset += 0x03;
    }
}

static void decode_pcm24(VGMSTREAMCHANNEL* stream, int32_t* buf, int channels, int first_sample, int samples_to_do, bool big_endian) {
    read_s32_t read_s24 = big_endian ? read_s24be : read_s24le;
    int s = 0;
    off_t offset = stream->offset + first_sample * 0x03;
    while (s < samples_to_do) {
        buf[s] = read_s24(offset, stream->streamfile);
        s += channels;
        offset += 0x03;
    }
}

static bool decode_buf_pcm24(VGMSTREAM* v, sbuf_t* sdst) {
    decode_state_t* ds = v->decode_state;
    bool big_endian = v->coding_type == coding_PCM24BE;

    if (sdst->fmt == SFMT_S16) {
        //TODO remove
        // using vgmstream without API (render_vgmstream) usually passes a S16 buf
        // could handle externally but blah blah, allow as-is for now
        int16_t* buffer = sdst->buf;
        buffer += sdst->filled * v->channels;
        for (int ch = 0; ch < v->channels; ch++) {
            decode_pcm24_i16(&v->ch[ch], buffer + ch, v->channels, ds->samples_into, ds->samples_left, big_endian);
        }
    }
    else {
        int32_t* buffer = sdst->buf;
        buffer += sdst->filled * v->channels;
        for (int ch = 0; ch < v->channels; ch++) {
            decode_pcm24(&v->ch[ch], buffer + ch, v->channels, ds->samples_into, ds->samples_left, big_endian);
        }
    }

    return true;
}

// TODO: remove after public API is used
static void decode_pcm32_i16(VGMSTREAMCHANNEL* stream, int16_t* buf, int channels, int first_sample, int samples_to_do, bool big_endian) {
    read_s32_t read_s32 = big_endian ? read_s32be : read_s32le;
    int s = 0;
    off_t offset = stream->offset + first_sample * 0x04;
    while (s < samples_to_do) {
        buf[s] = read_s32(offset, stream->streamfile) >> 16;
        s += channels;
        offset += 0x04;
    }
}

static void decode_pcm32(VGMSTREAMCHANNEL* stream, int32_t* buf, int channels, int first_sample, int samples_to_do, bool big_endian) {
    read_s32_t read_s32 = big_endian ? read_s32be : read_s32le;
    int s = 0;
    off_t offset = stream->offset + first_sample * 0x04;
    while (s < samples_to_do) {
        buf[s] = read_s32(offset, stream->streamfile);
        s += channels;
        offset += 0x04;
    }
}

static bool decode_buf_pcm32(VGMSTREAM* v, sbuf_t* sdst) {
    decode_state_t* ds = v->decode_state;
    bool big_endian = false;

    if (sdst->fmt == SFMT_S16) {
        //TODO remove
        // using vgmstream without API (render_vgmstream) usually passes a S16 buf
        // could handle externally but blah blah, allow as-is for now
        int16_t* buffer = sdst->buf;
        buffer += sdst->filled * v->channels;
        for (int ch = 0; ch < v->channels; ch++) {
            decode_pcm32_i16(&v->ch[ch], buffer + ch, v->channels, ds->samples_into, ds->samples_left, big_endian);
        }
    }
    else {
        int32_t* buffer = sdst->buf;
        buffer += sdst->filled * v->channels;
        for (int ch = 0; ch < v->channels; ch++) {
            decode_pcm32(&v->ch[ch], buffer + ch, v->channels, ds->samples_into, ds->samples_left, big_endian);
        }
    }

    return true;
}


int32_t pcm_bytes_to_samples(size_t bytes, int channels, int bits_per_sample) {
    if (channels <= 0 || bits_per_sample <= 0) return 0;
    return ((int64_t)bytes * 8) / channels / bits_per_sample;
}

#if 0
int32_t pcm32_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 32);
}
#endif

int32_t pcm24_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 24);
}

int32_t pcm16_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 16);
}

int32_t pcm8_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 8);
}

const codec_info_t pcm32_decoder = {
    .sample_type = SFMT_S32,
    .decode_buf = decode_buf_pcm32,
};

const codec_info_t pcm24_decoder = {
    .sample_type = SFMT_S24,
    .decode_buf = decode_buf_pcm24,
};

const codec_info_t pcmfloat_decoder = {
    .sample_type = SFMT_FLT,
    .decode_buf = decode_buf_pcmfloat,
};
