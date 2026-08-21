#include <stdlib.h>
#include "coding.h"
#include "libs/ongakukan_adp_lib.h"

#define ONGAKUKAN_MAX_DECODER_SAMPLES 2
struct ongakukan_adp_data {
    void* handle;
    int16_t* samples;
    int samples_filled;
    STREAMFILE* sf;
};

ongakukan_adp_data* init_ongakukan_adp(STREAMFILE* sf, int32_t data_offset, int32_t data_size, bool sound_is_adpcm) {
    ongakukan_adp_data* data = NULL;

    data = calloc(1, sizeof(ongakukan_adp_data));
    if (!data) goto fail;

    /* reopen STREAMFILE from here, then pass it as an argument for our init function. */
    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    data->handle = ongakukan_adpcm_init(data->sf, (long int)(data_offset), (long int)(data_size), sound_is_adpcm);
    if (!data->handle) goto fail;

    return data;
fail:
    free_ongakukan_adp(data);
    return NULL;
}

void decode_ongakukan_adp(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do) {
    ongakukan_adp_data* data = vgmstream->codec_data;

    int samples_done = 0;

    while (samples_done < samples_to_do) {
        if (data->samples_filled) {
            int s = ONGAKUKAN_MAX_DECODER_SAMPLES - data->samples_filled;
            while (data->samples_filled && samples_done < samples_to_do) {
                outbuf[samples_done] = data->samples[s++];
                samples_done++;
                data->samples_filled--;
            }
        }
        else {
            ongakukan_adpcm_decode_data(data->handle);

            data->samples = ongakukan_adpcm_get_sample_hist(data->handle); 
            data->samples_filled = ONGAKUKAN_MAX_DECODER_SAMPLES;
        }
    }
}

void reset_ongakukan_adp(ongakukan_adp_data* data) {
    if (!data) return;
    ongakukan_adpcm_reset(data->handle);
}

void seek_ongakukan_adp(ongakukan_adp_data* data, int32_t current_sample) {
    if (!data) return;
    ongakukan_adpcm_seek(data->handle, current_sample);
}

void free_ongakukan_adp(ongakukan_adp_data* data) {
    if (!data) return;
    close_streamfile(data->sf);
    ongakukan_adpcm_free(data->handle);
    free(data);
}

int32_t ongakukan_adp_get_samples(ongakukan_adp_data* data) {
    if (!data) return 0;
    return (int32_t)ongakukan_adpcm_get_num_samples(data->handle);
}
