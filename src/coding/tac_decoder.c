#include "coding.h"
#include "../base/codec_info.h"
#include "../base/decode_state.h"
#include "libs/tac_lib.h"


/* opaque struct */
typedef struct {
    uint8_t buf[TAC_BLOCK_SIZE];
    bool feed_block;
    off_t offset;

    float fbuf[TAC_FRAME_SAMPLES * TAC_CHANNELS];
    int discard;

    void* handle;
} tac_codec_data;

static void free_tac(void* priv_data) {
    tac_codec_data* data = priv_data;
    if (!data)
        return;

    tac_free(data->handle);
    free(data);
}

void* init_tac(STREAMFILE* sf) {
    tac_codec_data* data = NULL;
    int bytes;


    data = calloc(1, sizeof(tac_codec_data));
    if (!data) goto fail;

    bytes = read_streamfile(data->buf, 0x00, sizeof(data->buf), sf);
    data->handle = tac_init(data->buf, bytes);
    if (!data->handle) goto fail;

    data->feed_block = false; // ok to use first block
    data->offset = bytes;

    return data;
fail:
    free_tac(data);
    return NULL;
}


static int decode_frame(tac_codec_data* data) {
    int err = tac_decode_frame(data->handle, data->buf);

    if (err == TAC_PROCESS_NEXT_BLOCK) {
        data->feed_block = true;
        return 0;
    }

    if (err == TAC_PROCESS_DONE) {
        VGM_LOG("TAC: process done (EOF) %i\n", err);
        return -1; // shouldn't reach this
    }
    
    if (err != TAC_PROCESS_OK) {
        VGM_LOG("TAC: process error %i\n", err);
        return -1;
    }

    tac_get_samples_float(data->handle, data->fbuf);
    return TAC_FRAME_SAMPLES;
}

static bool read_frame(tac_codec_data* data, STREAMFILE* sf) {

    // new block must be read only when signaled by lib (a single block has N frames)
    if (!data->feed_block)
        return true;
    
    int bytes = read_streamfile(data->buf, data->offset, sizeof(data->buf), sf);
    data->offset += bytes;
    data->feed_block = 0;
    if (bytes <= 0) return false; // will read less that buf near EOF

    return true;
}

static bool decode_frame_tac(VGMSTREAM* v) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    tac_codec_data* data = v->codec_data;

    bool ok = read_frame(data, stream->streamfile);
    if (!ok)
        return false;

    decode_state_t* ds = v->decode_state;

    int samples = decode_frame(data);
    if (samples < 0)
        return false;

    sbuf_init_f16(&ds->sbuf, data->fbuf, samples, v->channels);
    ds->sbuf.filled = samples;

    // copy and let decoder handle
    if (data->discard) {
        ds->discard += data->discard;
        data->discard = 0;
    }

    return true;
}

static void reset_tac(void* priv_data) {
    tac_codec_data* data = priv_data;
    if (!data) return;

    tac_reset(data->handle);

    data->feed_block = true;
    data->offset = 0x00;
}

static void seek_tac(VGMSTREAM* v, int32_t num_sample) {
    tac_codec_data* data = v->codec_data;
    int32_t loop_sample;

    if (!data)
        return;

    const tac_header_t* hdr = tac_get_header(data->handle);

    loop_sample = (hdr->loop_frame - 1) * TAC_FRAME_SAMPLES + hdr->loop_discard;
    if (loop_sample == num_sample) {
        tac_set_loop(data->handle); /* direct looping */

        data->feed_block = true;
        data->offset = hdr->loop_offset;
        data->discard = hdr->loop_discard;
    }
    else {
        tac_reset(data->handle);

        data->feed_block = true;
        data->offset = 0x00;
        data->discard = num_sample;
    }
}

const codec_info_t tac_decoder = {
    .sample_type = SFMT_F16,
    .decode_frame = decode_frame_tac,
    .free = free_tac,
    .reset = reset_tac,
    .seek = seek_tac,
    //frame_samples = 1024,
};
