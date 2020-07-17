#include "coding.h"
#include "acm_decoder_libacm.h"
#include <stdio.h>

/* libacm 1.2 (despite what libacm.h says) from: https://github.com/markokr/libacm */

typedef struct {
    STREAMFILE* streamfile; /* reference */
    int offset;
} acm_io_config;


static int acm_read_streamfile(void* ptr, int size, int n, void* arg);
static int acm_seek_streamfile(void* arg, int offset, int whence);
static int acm_get_length_streamfile(void* arg);

acm_codec_data* init_acm(STREAMFILE* sf, int force_channel_number) {
    acm_codec_data* data = NULL;


    data = calloc(1,sizeof(acm_codec_data));
    if (!data) goto fail;

    data->io_config = calloc(1,sizeof(acm_io_config));
    if (!data->io_config) goto fail;

    data->streamfile = reopen_streamfile(sf, 0);
    if (!data->streamfile) goto fail;

    /* Setup libacm decoder, needs read callbacks and a parameter for said callbacks */
    {
        ACMStream* handle = NULL;
        int res;
        acm_io_config* io_config = data->io_config;
        acm_io_callbacks io_callbacks = {0};

        io_config->offset = 0;
        io_config->streamfile = data->streamfile;

        io_callbacks.read_func = acm_read_streamfile;
        io_callbacks.seek_func = acm_seek_streamfile;
        io_callbacks.close_func = NULL; /* managed in free_acm */
        io_callbacks.get_length_func = acm_get_length_streamfile;

        res = acm_open_decoder(&handle, io_config, io_callbacks, force_channel_number);
        if (res < 0) {
            VGM_LOG("ACM: failed opening libacm, error=%i\n", res);
            goto fail;
        }

        data->handle = handle;
    }


    return data;

fail:
    free_acm(data);
    return NULL;
}

void decode_acm(acm_codec_data* data, sample_t* outbuf, int32_t samples_to_do, int channelspacing) {
    ACMStream* acm = data->handle;
    int32_t samples_read = 0;

    while (samples_read < samples_to_do) {
        int32_t bytes_read_just_now = acm_read(
                acm,
                (char*)(outbuf+samples_read*channelspacing),
                (samples_to_do-samples_read)*sizeof(sample)*channelspacing,
                0,2,1);

        if (bytes_read_just_now > 0) {
            samples_read += bytes_read_just_now/sizeof(sample)/channelspacing;
        } else {
            return;
        }
    }
}

void reset_acm(acm_codec_data* data) {
    if (!data || !data->handle)
        return;

    acm_seek_pcm(data->handle, 0);
}

void free_acm(acm_codec_data* data) {
    if (!data)
        return;

    acm_close(data->handle);
    close_streamfile(data->streamfile);
    free(data->io_config);
    free(data);
}

STREAMFILE* acm_get_streamfile(acm_codec_data* data) {
    if (!data) return NULL;
    return data->streamfile;
}

/* ******************************* */

static int acm_read_streamfile(void *ptr, int size, int n, void *arg) {
    acm_io_config* config = arg;
    int bytes_read, items_read;

    bytes_read = read_streamfile(ptr,config->offset,size*n,config->streamfile);
    items_read = bytes_read / size;
    config->offset += bytes_read;

    return items_read;

}
static int acm_seek_streamfile(void *arg, int offset, int whence) {
    acm_io_config* config = arg;
    int base_offset, new_offset;

    switch (whence) {
        case SEEK_SET:
            base_offset = 0;
            break;
        case SEEK_CUR:
            base_offset = config->offset;
            break;
        case SEEK_END:
            base_offset = get_streamfile_size(config->streamfile);
            break;
        default:
            return -1;
            break;
    }

    new_offset = base_offset + offset;
    if (new_offset < 0 || new_offset > get_streamfile_size(config->streamfile)) {
        return -1; /* unseekable */
    } else {
        config->offset = new_offset;
        return 0;
    }
}
static int acm_get_length_streamfile(void *arg) {
    acm_io_config* config = arg;

    return get_streamfile_size(config->streamfile);
}
