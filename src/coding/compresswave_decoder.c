#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/compresswave_lib.h"


#define COMPRESSWAVE_MAX_FRAME_SAMPLES  512 // arbitrary, but should be multiple of 2 for 22050 mode
#define COMPRESSWAVE_MAX_CHANNELS  2 

typedef struct {
    STREAMFILE* sf;
    TCompressWaveData* handle;

    int16_t pbuf[COMPRESSWAVE_MAX_FRAME_SAMPLES * COMPRESSWAVE_MAX_CHANNELS];
    int discard;
} compresswave_codec_data;

static void reset_compresswave(void* priv_data) {
    compresswave_codec_data* data = priv_data;
    if (!data) return;

    /* actual way to reset internal flags */
    TCompressWaveData_Stop(data->handle);
    TCompressWaveData_Play(data->handle, 0);

    data->discard = 0;

    return;
}

static void free_compresswave(void* priv_data) {
    compresswave_codec_data* data = priv_data;
    if (!data)
        return;

    TCompressWaveData_Free(data->handle);

    close_streamfile(data->sf);
    free(data);
}

void* init_compresswave(STREAMFILE* sf) {
    compresswave_codec_data* data = NULL;

    data = calloc(1, sizeof(compresswave_codec_data));
    if (!data) goto fail;

    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    data->handle = TCompressWaveData_Create();
    if (!data->handle) goto fail;

    TCompressWaveData_LoadFromStream(data->handle, data->sf);

    reset_compresswave(data);

    return data;
fail:
    free_compresswave(data);
    return NULL;
}


static bool decode_frame_compresswave(VGMSTREAM* v) {
    compresswave_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;

    int samples = COMPRESSWAVE_MAX_FRAME_SAMPLES;
    //if (samples % 2 && samples > 1)
    //    samples -= 1; /* 22khz does 2 samples at once */


    uint32_t len = samples * sizeof(int16_t) * 2; /* forced stereo */

    int ok = TCompressWaveData_Rendering(data->handle, data->pbuf, len);
    if (!ok) return false;

    sbuf_init_s16(&ds->sbuf, data->pbuf, samples, v->channels);
    ds->sbuf.filled = ds->sbuf.samples;

    return true;
}

static void seek_compresswave(VGMSTREAM* v, int32_t num_sample) {
    compresswave_codec_data* data = v->codec_data;
    if (!data) return;

    reset_compresswave(data);
    data->discard += num_sample;
}

STREAMFILE* compresswave_get_streamfile(VGMSTREAM* v) {
    compresswave_codec_data* data = v->codec_data;
    if (!data) return NULL;
    return data->sf;
}

const codec_info_t compresswave_decoder = {
    .sample_type = SFMT_S16,
    .decode_frame = decode_frame_compresswave,
    .free = free_compresswave,
    .reset = reset_compresswave,
    .seek = seek_compresswave,
    //.frame_samples = 2-4 (lib handles arbitrary calls)
    //.frame_size = VBR / huffman codes
};
