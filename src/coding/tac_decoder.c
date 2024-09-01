#include "coding.h"
#include "coding_utils_samples.h"

#include "libs/tac_lib.h"


/* opaque struct */
struct tac_codec_data {
    /* config */
    int channels;
    int samples_discard;
    int encoder_delay;

    uint8_t buf[TAC_BLOCK_SIZE];
    bool feed_block;
    off_t offset;

    int16_t samples[TAC_FRAME_SAMPLES * TAC_CHANNELS];
    int frame_samples;

    /* frame state */
    s16buf_t sbuf;

    void* handle;
};


/* raw SPEEX */
tac_codec_data* init_tac(STREAMFILE* sf) {
    tac_codec_data* data = NULL;
    int bytes;


    data = calloc(1, sizeof(tac_codec_data));
    if (!data) goto fail;

    bytes = read_streamfile(data->buf, 0x00, sizeof(data->buf), sf);
    data->handle = tac_init(data->buf, bytes);
    if (!data->handle) goto fail;

    data->feed_block = false; /* ok to use current block */
    data->offset = bytes;
    data->channels = TAC_CHANNELS;
    data->frame_samples = TAC_FRAME_SAMPLES;

    data->encoder_delay = 0;
    data->samples_discard = data->encoder_delay;

    return data;
fail:
    free_tac(data);
    return NULL;
}


static bool decode_frame(tac_codec_data* data) {
    int err;

    data->sbuf.samples = data->samples;
    data->sbuf.channels = data->channels;
    data->sbuf.filled = 0;

    err = tac_decode_frame(data->handle, data->buf);

    if (err == TAC_PROCESS_NEXT_BLOCK) {
        data->feed_block = true;
        return true;
    }

    if (err == TAC_PROCESS_DONE) {
        VGM_LOG("TAC: process done (EOF) %i\n", err);
        return false; /* shouldn't reach this */
    }
    
    if (err != TAC_PROCESS_OK) {
        VGM_LOG("TAC: process error %i\n", err);
        return false;
    }


    tac_get_samples_pcm16(data->handle, data->sbuf.samples);
    data->sbuf.filled = data->frame_samples;

    return true;
}

static bool read_frame(tac_codec_data* data, STREAMFILE* sf) {

    /* new block must be read only when signaled by lib */
    if (!data->feed_block)
        return true;
    
    int bytes = read_streamfile(data->buf, data->offset, sizeof(data->buf), sf);
    data->offset += bytes;
    data->feed_block = 0;
    if (bytes <= 0) return false; /* can read less that buf near EOF */

    return true;
}

void decode_tac(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do) {
    VGMSTREAMCHANNEL* stream = &vgmstream->ch[0];
    tac_codec_data* data = vgmstream->codec_data;
    bool ok;


    while (samples_to_do > 0) {
        s16buf_t* sbuf = &data->sbuf;

        if (sbuf->filled <= 0) {
            ok = read_frame(data, stream->streamfile);
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
    VGM_LOG("TAC: decode fail at %x, missing %i samples\n", (uint32_t)data->offset, samples_to_do);
    s16buf_silence(&outbuf, &samples_to_do, data->sbuf.channels);
}


void reset_tac(tac_codec_data* data) {
    if (!data) return;

    tac_reset(data->handle);

    data->feed_block = true;
    data->offset = 0x00;
    data->sbuf.filled = 0;
    data->samples_discard = data->encoder_delay;
}

void seek_tac(tac_codec_data* data, int32_t num_sample) {
    int32_t loop_sample;
    const tac_header_t* hdr;

    if (!data)
        return;

    hdr = tac_get_header(data->handle);

    loop_sample = (hdr->loop_frame - 1) * TAC_FRAME_SAMPLES + hdr->loop_discard;
    if (loop_sample == num_sample) {
        tac_set_loop(data->handle); /* direct looping */

        data->feed_block = true;
        data->offset = hdr->loop_offset;
        data->sbuf.filled = 0;
        data->samples_discard = hdr->loop_discard;
    }
    else {
        tac_reset(data->handle);

        data->feed_block = true;
        data->offset = 0x00;
        data->sbuf.filled = 0;
        data->samples_discard = num_sample;
    }
}

void free_tac(tac_codec_data* data) {
    if (!data)
        return;

    tac_free(data->handle);
    free(data);
}
