#include "coding.h"
#include "coding_utils_samples.h"

#ifdef VGM_USE_SPEEX
#include "speex/speex.h"

#define SPEEX_MAX_FRAME_SIZE  0x100  /* frame sizes are stored in a byte */
#define SPEEX_MAX_FRAME_SAMPLES  640  /* nb=160, wb/uwb=320*2 */
#define SPEEX_CTL_OK  0  /* -1=request unknown, -2=invalid param */
#define SPEEX_DECODE_OK  0  /* -1 for end of stream, -2 corrupt stream */


/* opaque struct */
struct speex_codec_data {
    /* config */
    int channels;
    int samples_discard;
    int encoder_delay;

    uint8_t buf[SPEEX_MAX_FRAME_SIZE];
    uint8_t frame_size;

    int16_t* samples;
    int frame_samples;

    /* frame state */
    s16buf_t sbuf;

    void* state;
    SpeexBits bits;
};


/* raw SPEEX */
speex_codec_data* init_speex_ea(int channels) {
    int res, sample_rate;
    speex_codec_data* data = NULL;


    data = calloc(1, sizeof(speex_codec_data));
    if (!data) goto fail;

    //TODO: EA uses N decoders, unknown layout (known samples are mono)
    data->channels = channels;
    if (channels != 1)
        goto fail;

    /* Modes: narrowband=nb, wideband=wb, ultrawideband=uwb modes.
     * EASpeex seem to always use uwb so use that for now until config is needed.
     * Examples normally use &speex_*_mode but export seem problematic? */
    data->state = speex_decoder_init(speex_lib_get_mode(SPEEX_MODEID_UWB));
    if (!data->state) goto fail;

    speex_bits_init(&data->bits);

    res = speex_decoder_ctl(data->state, SPEEX_GET_FRAME_SIZE, &data->frame_samples);
    if (res != SPEEX_CTL_OK) goto fail;

    if (data->frame_samples > SPEEX_MAX_FRAME_SAMPLES)
        goto fail;

    /* forced in EA's code, doesn't seem to affect decoding (all EAAC headers use this rate too) */
    sample_rate = 32000;
    res = speex_decoder_ctl(data->state, SPEEX_SET_SAMPLING_RATE, &sample_rate);
    if (res != SPEEX_CTL_OK) goto fail;

    /* default "latency" for EASpeex */
    data->encoder_delay = 509;
    data->samples_discard = data->encoder_delay;

    data->samples = malloc(channels * data->frame_samples * sizeof(int16_t));
    if (!data->samples) goto fail;

    return data;

fail:
    free_speex(data);
    return NULL;
}


static int decode_frame(speex_codec_data* data) {
    int res;

    data->sbuf.samples = data->samples;
    data->sbuf.channels = 1;
    data->sbuf.filled = 0;

    speex_bits_read_from(&data->bits, (const char*)data->buf, data->frame_size);

    res = speex_decode_int(data->state, &data->bits, data->sbuf.samples);
    if (res != SPEEX_DECODE_OK) goto fail;

    data->sbuf.filled = data->frame_samples;

    return 1;
fail:
    return 0;
}

/* for simple style speex (seen in EA-Speex and libspeex's sampledec.c) */
static int read_frame(speex_codec_data* data, VGMSTREAMCHANNEL* stream) {
    size_t bytes;

    data->frame_size = read_u8(stream->offset, stream->streamfile);
    stream->offset += 0x01;
    if (data->frame_size == 0) goto fail;

    bytes = read_streamfile(data->buf, stream->offset, data->frame_size, stream->streamfile);
    stream->offset += data->frame_size;
    if (bytes != data->frame_size) goto fail;

    return 1;
fail:
    return 0;
}

void decode_speex(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do) {
    VGMSTREAMCHANNEL* stream = &vgmstream->ch[0];
    speex_codec_data* data = vgmstream->codec_data;
    int ok;


    while (samples_to_do > 0) {
        s16buf_t* sbuf = &data->sbuf;

        if (sbuf->filled <= 0) {
            ok = read_frame(data, stream);
            if (!ok) goto fail;

            ok = decode_frame(data);
            if (!ok) goto fail;
        }

        if (data->samples_discard)
            s16buf_discard(&outbuf, sbuf, &data->samples_discard);
        else
            s16buf_consume(&outbuf, sbuf, &samples_to_do);
    }

    return;

fail:
    /* on error just put some 0 samples */
    VGM_LOG("SPEEX: decode fail at %x, missing %i samples\n", (uint32_t)stream->offset, samples_to_do);
    s16buf_silence(&outbuf, &samples_to_do, data->channels);
}


void reset_speex(speex_codec_data* data) {
    int res;

    if (!data) return;

    res = speex_decoder_ctl(data->state, SPEEX_RESET_STATE, NULL);
    if (res != SPEEX_CTL_OK) goto fail;

    data->sbuf.filled = 0;
    data->samples_discard = data->encoder_delay;

    return;
fail:
    return; /* ? */
}

void seek_speex(VGMSTREAM* vgmstream, int32_t num_sample) {
    speex_codec_data* data = vgmstream->codec_data;
    if (!data) return;

    reset_speex(data);
    data->samples_discard += num_sample;

    /* loop offsets are set during decode; force them to stream start so discard works */
    if (vgmstream->loop_ch)
        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
}

void free_speex(speex_codec_data* data) {
    if (!data)
        return;

    if (data->state) {
        speex_decoder_destroy(data->state);
        speex_bits_destroy(&data->bits);
    }

    free(data->samples);
    free(data);
}
#endif
