#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"

#ifdef VGM_USE_SPEEX
#include "speex/speex.h"

#define SPEEX_MAX_FRAME_SIZE  0x100  /* frame sizes are stored in a byte */
#define SPEEX_MAX_FRAME_SAMPLES  640  /* nb=160, wb/uwb=320*2 */
#define SPEEX_MAX_CHANNELS  1  /* nb=160, wb/uwb=320*2 */
#define SPEEX_CTL_OK  0  /* -1=request unknown, -2=invalid param */
#define SPEEX_DECODE_OK  0  /* -1 for end of stream, -2 corrupt stream */


typedef enum { EA, TORUS } speex_type_t;

/* opaque struct */
typedef struct {
    speex_type_t type;

    /* config */
    int channels;
    int encoder_delay;
    int samples_discard;

    uint8_t buf[SPEEX_MAX_FRAME_SIZE];
    int frame_size;

    short pbuf[SPEEX_MAX_FRAME_SAMPLES * SPEEX_MAX_CHANNELS];
    int frame_samples;

    /* frame state */
    void* handle;
    SpeexBits bits;
} speex_codec_data;


static void free_speex(void* priv_data) {
    speex_codec_data* data = priv_data;
    if (!data)
        return;

    if (data->handle) {
        speex_decoder_destroy(data->handle);
        speex_bits_destroy(&data->bits);
    }

    free(data);
}


// raw-ish SPEEX (without Ogg)
static speex_codec_data* init_speex(speex_type_t type, int channels) {
    int res, sample_rate;
    speex_codec_data* data = NULL;

    // not seen, unknown layout (known cases use xN mono decoders)
    if (channels > SPEEX_MAX_CHANNELS)
        return NULL;

    data = calloc(1, sizeof(speex_codec_data));
    if (!data) goto fail;

    data->type = type;
    data->channels = channels;

    // Modes: narrowband=nb, wideband=wb, ultrawideband=uwb modes.
    // Known decoders seem to always use uwb so use that for now until config is needed.
    // Examples normally use &speex_*_mode but exports seem problematic?
    data->handle = speex_decoder_init(speex_lib_get_mode(SPEEX_MODEID_UWB));
    if (!data->handle) goto fail;

    speex_bits_init(&data->bits);

    res = speex_decoder_ctl(data->handle, SPEEX_GET_FRAME_SIZE, &data->frame_samples);
    if (res != SPEEX_CTL_OK) goto fail;

    if (data->frame_samples > SPEEX_MAX_FRAME_SAMPLES)
        goto fail;

    // forced in EA's code, doesn't seem to affect decoding (all EAAC headers use this rate too)
    sample_rate = 32000;
    res = speex_decoder_ctl(data->handle, SPEEX_SET_SAMPLING_RATE, &sample_rate);
    if (res != SPEEX_CTL_OK) goto fail;

    /* default "latency" for EASpeex */
    data->encoder_delay = 509;
    data->samples_discard = data->encoder_delay;

    return data;

fail:
    free_speex(data);
    return NULL;
}

void* init_speex_ea(int channels) {
    return init_speex(EA, channels);
}

void* init_speex_torus(int channels) {
    return init_speex(TORUS, channels);
}


static int decode_frame(speex_codec_data* data) {
    speex_bits_read_from(&data->bits, (const char*)data->buf, data->frame_size);

    // speex_decode() returns samples (F32), but internally speex decodes into pcm16
    int res = speex_decode_int(data->handle, &data->bits, data->pbuf);
    if (res != SPEEX_DECODE_OK) return -1;

    return data->frame_samples;
}

// for simple style speex (seen in EA-Speex and libspeex's sampledec.c)
static bool read_frame(speex_codec_data* data, VGMSTREAMCHANNEL* stream) {
    switch(data->type) {
        case EA:
            data->frame_size = read_u8(stream->offset, stream->streamfile);
            stream->offset += 0x01;
            break;
        case TORUS:
            data->frame_size = read_u16le(stream->offset, stream->streamfile);
            stream->offset += 0x02;
            break;
        default:
            break;
    }
    if (data->frame_size == 0)
        return false;

    size_t bytes = read_streamfile(data->buf, stream->offset, data->frame_size, stream->streamfile);
    stream->offset += data->frame_size;

    if (bytes != data->frame_size)
        return false;

    return true;
}

static bool decode_frame_speex(VGMSTREAM* v) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    speex_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;

    bool ok = read_frame(data, stream);
    if (!ok) return false;

    int samples = decode_frame(data);
    if (samples < 0) return false;

    sbuf_init_s16(&ds->sbuf, data->pbuf, samples, data->channels);
    ds->sbuf.filled = ds->sbuf.samples;

    if (data->samples_discard) {
        ds->discard += data->samples_discard;
        data->samples_discard = 0;
    }

    return true;
}

static void reset_speex(void* priv_data) {
    speex_codec_data* data = priv_data;
    if (!data)
        return;

    int res = speex_decoder_ctl(data->handle, SPEEX_RESET_STATE, NULL);
    if (res != SPEEX_CTL_OK)
        return; //???

    data->samples_discard = data->encoder_delay;
}

static void seek_speex(VGMSTREAM* v, int32_t num_sample) {
    decode_state_t* ds = v->decode_state;
    speex_codec_data* data = v->codec_data;
    if (!data) return;

    reset_speex(data);

    ds->discard = num_sample;

    // loop offsets are set during decode; force them to stream start so discard works
    if (v->loop_ch)
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
}

const codec_info_t speex_decoder = {
    .sample_type = SFMT_S16,
    .decode_frame = decode_frame_speex,
    .free = free_speex,
    .reset = reset_speex,
    .seek = seek_speex,
};
#endif
