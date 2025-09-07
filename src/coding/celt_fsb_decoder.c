#ifdef VGM_USE_CELT
#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "celt/celt_fsb.h"

#define FSB_CELT_0_06_1_VERSION 0x80000009 /* libcelt-0.6.1 */
#define FSB_CELT_0_11_0_VERSION 0x80000010 /* libcelt-0.11.0 */
#define FSB_CELT_SAMPLES_PER_FRAME 512
#define FSB_CELT_INTERNAL_SAMPLE_RATE 44100
#define FSB_CELT_MAX_DATA_SIZE 0x200 /* from 0x2e~0x172/1d0, all files are CBR though */

typedef enum { CELT_0_06_1, CELT_0_11_0} celt_lib_t;

/* opaque struct */
typedef struct {
    uint8_t buf[FSB_CELT_MAX_DATA_SIZE];
    int frame_size; // current size

    int16_t* sbuf;

    int discard;

    int channel_mode;
    celt_lib_t version;
    void* mode_handle;
    void* decoder_handle;
} celt_codec_data;

static void free_celt_fsb(void* priv_data) {
    celt_codec_data* data = priv_data;
    if (!data) return;

    switch(data->version) {
        case CELT_0_06_1:
            if (data->decoder_handle) celt_decoder_destroy_0061(data->decoder_handle);
            if (data->mode_handle) celt_mode_destroy_0061(data->mode_handle);
            break;

        case CELT_0_11_0:
            if (data->decoder_handle) celt_decoder_destroy_0110(data->decoder_handle);
            if (data->mode_handle) celt_mode_destroy_0110(data->mode_handle);
            break;

        default:
            break;
    }

    free(data->sbuf);
    free(data);
}

/* FSB CELT, frames with custom header and standard data (API info from FMOD DLLs).
 * FMOD used various libcelt versions, thus some tweaks are needed for them to coexist. */
static void* init_celt_fsb(int channels, celt_lib_t version) {
    int error = 0, lib_version = 0;
    celt_codec_data* data = NULL;


    data = calloc(1, sizeof(celt_codec_data));
    if (!data) goto fail;

    data->channel_mode = channels; /* should be 1/2, or rejected by libcelt */
    data->version = version;

    switch(data->version) {
        case CELT_0_06_1: // older FSB4 (FMOD ~4.33)
            data->mode_handle = celt_mode_create_0061(FSB_CELT_INTERNAL_SAMPLE_RATE, data->channel_mode, FSB_CELT_SAMPLES_PER_FRAME, &error);
            if (!data->mode_handle || error != CELT_OK) goto fail;

            error = celt_mode_info_0061(data->mode_handle, CELT_GET_BITSTREAM_VERSION, &lib_version);
            if (error != CELT_OK || lib_version != FSB_CELT_0_06_1_VERSION) goto fail;

            data->decoder_handle = celt_decoder_create_0061(data->mode_handle);
            if (!data->decoder_handle) goto fail;
            break;

        case CELT_0_11_0: // newer FSB4 (FMOD ~4.34), FSB5
            data->mode_handle = celt_mode_create_0110(FSB_CELT_INTERNAL_SAMPLE_RATE, FSB_CELT_SAMPLES_PER_FRAME, &error); /* "custom" and not ok? */
            if (!data->mode_handle || error != CELT_OK) goto fail;

            error = celt_mode_info_0110(data->mode_handle, CELT_GET_BITSTREAM_VERSION, &lib_version);
            if (error != CELT_OK || lib_version != FSB_CELT_0_11_0_VERSION) goto fail;

            data->decoder_handle = celt_decoder_create_custom_0110(data->mode_handle, data->channel_mode, &error);
            if (!data->decoder_handle || error != CELT_OK) goto fail;
            break;

        default:
            goto fail;
    }

    data->sbuf = calloc(data->channel_mode * FSB_CELT_SAMPLES_PER_FRAME, sizeof(int16_t));
    if (!data->sbuf) goto fail;

    // ~128 samples of encoder delay, but FMOD DLLs don't discard them?

    return data;

fail:
    free_celt_fsb(data);
    return NULL;
}

void* init_celt_fsb_v1(int channels) {
    return init_celt_fsb(channels, CELT_0_06_1);
}

void* init_celt_fsb_v2(int channels) {
    return init_celt_fsb(channels, CELT_0_11_0);
}

// read and decode one raw block and advance offsets
static bool read_frame(VGMSTREAM* v) {
    VGMSTREAMCHANNEL* vs = &v->ch[0];
    celt_codec_data* data = v->codec_data;

    // FSB DLLs do seem to check this fixed value
    if (read_u32be(vs->offset + 0x00, vs->streamfile) != 0x17C30DF3)
        return false;

    data->frame_size = read_u32le(vs->offset + 0x04, vs->streamfile);
    if (data->frame_size > FSB_CELT_MAX_DATA_SIZE)
        return false;

    int bytes = read_streamfile(data->buf, vs->offset+0x08, data->frame_size, vs->streamfile);

    vs->offset += 0x04 + 0x04 + data->frame_size;

    return (bytes == data->frame_size);
}

static int decode(VGMSTREAM* v) {
    celt_codec_data* data = v->codec_data;

    int samples = FSB_CELT_SAMPLES_PER_FRAME;
    int status;
    switch(data->version) {
        case CELT_0_06_1:
            status = celt_decode_0061(data->decoder_handle, data->buf, data->frame_size, data->sbuf);
            if (status != CELT_OK)
                return -1;
            return samples;

        case CELT_0_11_0:
            status = celt_decode_0110(data->decoder_handle, data->buf, data->frame_size, data->sbuf, samples);
            if (status != CELT_OK)
                return -1;
            return samples;

        default:
            return -1;
    }
}

static bool decode_frame_celt_fsb(VGMSTREAM* v) {
    bool ok = read_frame(v);
    if (!ok)
        return false;

    int samples = decode(v);
    if (samples <= 0)
        return false;

    decode_state_t* ds = v->decode_state;
    celt_codec_data* data = v->codec_data;

    sbuf_init_s16(&ds->sbuf, data->sbuf, samples, v->channels);
    ds->sbuf.filled = samples;

    if (data->discard) {
        ds->discard += data->discard;
        data->discard = 0;
    }

    return true;
}


static void reset_celt_fsb(void* priv_data) {
    celt_codec_data* data = priv_data;
    if (!data) return;

    // recreate decoder (mode should not change)
    switch(data->version) {
        case CELT_0_06_1:
            if (data->decoder_handle)
                celt_decoder_destroy_0061(data->decoder_handle);

            data->decoder_handle = celt_decoder_create_0061(data->mode_handle);
            if (!data->decoder_handle) goto fail;
            break;

        case CELT_0_11_0:
            if (data->decoder_handle)
                celt_decoder_destroy_0110(data->decoder_handle);

            data->decoder_handle = celt_decoder_create_custom_0110(data->mode_handle, data->channel_mode, NULL);
            if (!data->decoder_handle) goto fail;
            break;

        default:
            goto fail;
    }

    data->discard = 0;

    return;
fail:
    return; /* decode calls should fail... */
}

static void seek_celt_fsb(VGMSTREAM* v, int32_t num_sample) {
    celt_codec_data* data = v->codec_data;
    if (!data) return;

    reset_celt_fsb(data);

    data->discard = num_sample;

    /* loop offsets are set during decode; force them to stream start so discard works */
    if (v->loop_ch)
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
}


const codec_info_t celt_fsb_decoder = {
    .sample_type = SFMT_S16, // decoder doesn't return float
    .decode_frame = decode_frame_celt_fsb,
    .free = free_celt_fsb,
    .reset = reset_celt_fsb,
    .seek = seek_celt_fsb,
};
#endif
