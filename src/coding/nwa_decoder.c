#include "libs/nwa_lib.h"
#include "coding.h"


struct nwa_codec_data {
    STREAMFILE* sf;
    NWAData* nwa;
};

/* interface to vgmstream */
void decode_nwa(nwa_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {
    NWAData* nwa = data->nwa;

    while (samples_to_do > 0) {
        if (nwa->samples_in_buffer > 0) {
            int32_t samples_to_read = nwa->samples_in_buffer / nwa->channels;
            if (samples_to_read > samples_to_do)
                samples_to_read = samples_to_do;

            memcpy(outbuf, nwa->outdata_readpos, sizeof(sample_t) * samples_to_read * nwa->channels);

            nwa->outdata_readpos += samples_to_read * nwa->channels;
            nwa->samples_in_buffer -= samples_to_read * nwa->channels;
            outbuf += samples_to_read * nwa->channels;
            samples_to_do -= samples_to_read;
        }
        else {
            int err = nwalib_decode(data->sf, nwa);
            if (err < 0) {
                VGM_LOG("NWA: decoding error\n");
                return;
            }
        }
    }
}


nwa_codec_data* init_nwa(STREAMFILE* sf) {
    nwa_codec_data* data = NULL;

    data = calloc(1, sizeof(nwa_codec_data));
    if (!data) goto fail;

    data->nwa = nwalib_open(sf);
    if (!data->nwa) goto fail;

    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    return data;

fail:
    free_nwa(data);
    return NULL;
}

void seek_nwa(nwa_codec_data* data, int32_t sample) {
    if (!data) return;

    nwalib_seek(data->sf, data->nwa, sample);
}

void reset_nwa(nwa_codec_data* data) {
    if (!data) return;

    nwalib_reset(data->nwa);
}

void free_nwa(nwa_codec_data* data) {
    if (!data) return;

    close_streamfile(data->sf);
    nwalib_close(data->nwa);
    free(data);
}

STREAMFILE* nwa_get_streamfile(nwa_codec_data* data) {
     if (!data) return NULL;

     return data->sf;
 }
