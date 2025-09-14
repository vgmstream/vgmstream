#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/binka_dec.h"

// observed max is ~0x1200 in 8ch for BCF1 and ~0x900 in 2ch files for UEBA (higher encoding quality modes?)
// (there is 32 bit field in each format for this)
#define MAX_FRAME_SIZE_CHANNEL 0x800


typedef struct {
    uint8_t* buf;
    uint32_t buf_size;
    uint32_t filled;
    int samples;
} packet_t;

/* opaque struct */
typedef struct {
    float* fbuf;

    packet_t pkt;
    void* handle;
} binka_codec_data;


static void free_binka(void* priv_data) {
    binka_codec_data* data = priv_data;
    if (!data) return;

    if (data->handle)
        binka_free(data->handle);
    free(data->pkt.buf);
    free(data->fbuf);
    free(data);
}

static void* init_binka(int sample_rate, int channels, binka_mode_t mode) {
    binka_codec_data* data = NULL;
    int frame_samples;

    data = calloc(1, sizeof(binka_codec_data));
    if (!data) goto fail;

    data->handle = binka_init(sample_rate, channels, mode);
    if (!data->handle) goto fail;

    frame_samples = binka_get_frame_samples(data->handle);
    if (frame_samples <= 0) goto fail;

    data->pkt.buf_size = MAX_FRAME_SIZE_CHANNEL * channels;
    data->pkt.buf = calloc(data->pkt.buf_size, sizeof(uint8_t));
    if (!data->pkt.buf) goto fail;

    data->fbuf = calloc(frame_samples * channels, sizeof(float));
    if (!data->fbuf) goto fail;

    return data;
fail:
    free_binka(data);
    return NULL;
}

void* init_binka_bcf1(int sample_rate, int channels) {
    return init_binka(sample_rate, channels, BINKA_BFC);
}

void* init_binka_ueba(int sample_rate, int channels) {
    return init_binka(sample_rate, channels, BINKA_UEBA);
}

// reads 1 binka packets (with sync, unlike Bink Video's audio packets)
static bool read_frame(VGMSTREAM* v, packet_t* pkt) {
    binka_codec_data* data = v->codec_data;
    VGMSTREAMCHANNEL* vs = &v->ch[0];

    uint32_t header = read_u32le(vs->offset, vs->streamfile);

    if (header == get_id32be("KEES")) {
        /*
            stream SEEK chunk for next N packets (UEBA v2/RADA only), may appear multiple times through the file
            00 SEEK
            04 0x00/01 (version?)
            05 block samples (minus overlap)
            07 unknown (usually 0, 0x6900)
            0b seek entries
            0f seek table (0x02 per entry)
            xx data 
        */

        int entries = read_s32le(vs->offset + 0x0b, vs->streamfile);
        if (entries <= 0)
            return false;
        vs->offset += 0x0f + entries * 0x02;
        pkt->filled = 0;
        return true;
    }

    uint16_t sync = (header >> 0) & 0xFFFF;
    uint16_t packet_size = (header >> 16) & 0xFFFF;
    vs->offset += 0x04;

    if (sync != 0x9999) {
        VGM_LOG("BINKA: missing sync at %x\n", (uint32_t)vs->offset);
        return false;
    }

    int packet_samples = 0;
    if (packet_size == 0xFFFF) { // last packet marker? (used in UEBA)
        packet_size     = read_u16le(vs->offset + 0x00, vs->streamfile);
        packet_samples  = read_u16le(vs->offset + 0x02, vs->streamfile); // less samples than max
        vs->offset += 0x04;
    }

    if (packet_size > data->pkt.buf_size) {
        VGM_LOG("BINKA: packet too large, %x vs %x\n", packet_size, data->pkt.buf_size);
        return false;
    }

    data->pkt.filled = read_streamfile(data->pkt.buf, vs->offset, packet_size, vs->streamfile);
    if (data->pkt.filled != packet_size)
        return false;
    data->pkt.samples = packet_samples;

    vs->offset += packet_size;
    return true;
}

static bool decode_frame_binka(VGMSTREAM* v) {
    binka_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;

    bool ok = read_frame(v, &data->pkt);
    if (!ok)
        return false;

    if (data->pkt.filled == 0) {
        ds->sbuf.filled = 0; //nothing to decoder in this call
        return true;
    }

    int samples = binka_decode(data->handle, data->pkt.buf, data->pkt.filled, data->fbuf);
    if (samples < 0)
        return false;

    // last packet may emmit less samples
    if (data->pkt.samples && samples > data->pkt.samples)
        samples = data->pkt.samples;

    sbuf_init_f16(&ds->sbuf, data->fbuf, samples, v->channels);
    ds->sbuf.filled = samples;

    return true;
}

static void reset_binka(void* priv_data) {
    binka_codec_data* data = priv_data;
    if (!data || !data->handle) return;
    
    binka_reset(data->handle);
}

static void seek_binka(VGMSTREAM* v, int32_t num_sample) {
    binka_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    if (!data) return;

    reset_binka(data);

    //TODO use seek table (and/or read N packets samples until target)
    ds->discard = num_sample;
    if (v->loop_ch) {
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
    }
}

const codec_info_t binka_decoder = {
    .sample_type = SFMT_F16,
    .decode_frame = decode_frame_binka,
    .free = free_binka,
    .reset = reset_binka,
    .seek = seek_binka,
};
