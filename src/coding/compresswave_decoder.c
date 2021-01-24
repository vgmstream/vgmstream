#include "coding.h"
#include "coding_utils_samples.h"
#include "compresswave_decoder_lib.h"


#define COMPRESSWAVE_MAX_FRAME_SAMPLES  0x1000  /* arbitrary but should be multiple of 2 for 22050 mode */

/* opaque struct */
struct compresswave_codec_data {
    /* config */
    STREAMFILE* sf;
    TCompressWaveData* cw;

    /* frame state */
    int16_t* samples;
    int frame_samples;

    /* frame state */
    s16buf_t sbuf;
    int samples_discard;
};


compresswave_codec_data* init_compresswave(STREAMFILE* sf) {
    compresswave_codec_data* data = NULL;

    data = calloc(1, sizeof(compresswave_codec_data));
    if (!data) goto fail;

    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    data->frame_samples = COMPRESSWAVE_MAX_FRAME_SAMPLES;
    data->samples = malloc(2 * data->frame_samples * sizeof(int16_t)); /* always stereo */
    if (!data->samples) goto fail;


    data->cw = TCompressWaveData_Create();
    if (!data->cw) goto fail;

    TCompressWaveData_LoadFromStream(data->cw, data->sf);

    reset_compresswave(data);

    return data;
fail:
    free_compresswave(data);
    return NULL;
}


static int decode_frame(compresswave_codec_data* data, int32_t samples_to_do) {
    uint32_t Len;
    int ok;

    data->sbuf.samples = data->samples;
    data->sbuf.channels = 2;
    data->sbuf.filled = 0;

    if (samples_to_do > data->frame_samples)
        samples_to_do = data->frame_samples;
    if (samples_to_do % 2 && samples_to_do > 1)
        samples_to_do -= 1; /* 22khz does 2 samples at once */

    Len = samples_to_do * sizeof(int16_t) * 2; /* forced stereo */

    ok = TCompressWaveData_Rendering(data->cw, data->sbuf.samples, Len);
    if (!ok) goto fail;

    data->sbuf.filled = samples_to_do;

    return 1;
fail:
    return 0;
}


void decode_compresswave(compresswave_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {
    int ok;


    while (samples_to_do > 0) {
        s16buf_t* sbuf = &data->sbuf;

        if (sbuf->filled <= 0) {
            ok = decode_frame(data, samples_to_do);
            if (!ok) goto fail;
        }

        if (data->samples_discard)
            s16buf_discard(&outbuf, sbuf, &data->samples_discard);
        else
            s16buf_consume(&outbuf, sbuf, &samples_to_do);
    }

    return;

fail:
    VGM_LOG("COMPRESSWAVE: decode fail, missing %i samples\n", samples_to_do);
    s16buf_silence(&outbuf, &samples_to_do, 2);
}


void reset_compresswave(compresswave_codec_data* data) {
    if (!data) return;

    /* actual way to reset internal flags */
    TCompressWaveData_Stop(data->cw);
    TCompressWaveData_Play(data->cw, 0);

    data->sbuf.filled = 0;
    data->samples_discard = 0;

    return;
}

void seek_compresswave(compresswave_codec_data* data, int32_t num_sample) {
    if (!data) return;

    reset_compresswave(data);
    data->samples_discard += num_sample;
}

void free_compresswave(compresswave_codec_data* data) {
    if (!data)
        return;

    TCompressWaveData_Free(data->cw);

    close_streamfile(data->sf);
    free(data->samples);
    free(data);
}

STREAMFILE* compresswave_get_streamfile(compresswave_codec_data* data) {
    if (!data) return NULL;
    return data->sf;
}
