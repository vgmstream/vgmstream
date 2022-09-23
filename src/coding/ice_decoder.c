#include "coding.h"
#include "ice_decoder_icelib.h"


typedef struct {
    STREAMFILE* sf;
    int offset;
} icelib_io_t;

struct ice_codec_data {
    STREAMFILE* sf;
    int channels;
    icesnd_handle_t* ctx;
    icelib_io_t io;
};

static void icelib_set_callbacks(icesnd_callback_t* cb, STREAMFILE* sf, icelib_io_t* io);

ice_codec_data* init_ice(STREAMFILE* sf, int subsong) {
    ice_codec_data* data = NULL;

    data = calloc(1, sizeof(ice_codec_data));
    if (!data) goto fail;

    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    {
        icesnd_callback_t cb = {0};
        icesnd_info_t info = {0};
        int err;

        icelib_set_callbacks(&cb, data->sf, &data->io);

        data->ctx = icesnd_init(subsong, &cb);
        if (!data->ctx) goto fail;

        err = icesnd_info(data->ctx, &info);
        if (err < ICESND_RESULT_OK) goto fail;

        data->channels = info.channels;
    }

    return data;
fail:
    free_ice(data);
    return NULL;
}

void decode_ice(ice_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {
    int channels = data->channels;

    while (samples_to_do > 0) {
        int done = icesnd_decode(data->ctx, outbuf, samples_to_do);
        if (done <= 0) goto decode_fail;

        outbuf += done * channels;
        samples_to_do -= done;
    }

    return;
    
decode_fail:
    VGM_LOG("ICE: decode error\n");
    memset(outbuf, 0, samples_to_do * channels * sizeof(sample_t));
}

void reset_ice(ice_codec_data* data) {
    if (!data) return;

    icesnd_reset(data->ctx, 0);
}

void seek_ice(ice_codec_data* data, int32_t num_sample) {
    if (!data) return;

    //todo discard (this should only be called when looping)
    icesnd_reset(data->ctx, 1);
}

void free_ice(ice_codec_data* data) {
    if (!data) return;

    close_streamfile(data->sf);
    icesnd_free(data->ctx);
    free(data);
}

/* ************************* */

static int icelib_read(void* dst, int size, int n, void* arg) {
    icelib_io_t* io = arg;
    int bytes_read, items_read;

    bytes_read = read_streamfile(dst, io->offset, size * n, io->sf);
    items_read = bytes_read / size;
    io->offset += bytes_read;

    return items_read;
}

static int icelib_seek(void* arg, int offset, int whence) {
    icelib_io_t* io = arg;
    int base_offset, new_offset;

    switch (whence) {
        case ICESND_SEEK_SET:
            base_offset = 0;
            break;
        case ICESND_SEEK_CUR:
            base_offset = io->offset;
            break;
        case ICESND_SEEK_END:
            base_offset = get_streamfile_size(io->sf);
            break;
        default:
            return -1;
            break;
    }

    new_offset = base_offset + offset;
    if (new_offset < 0 /*|| new_offset > get_streamfile_size(config->sf)*/) {
        return -1; /* unseekable */
    }
    else {
        io->offset = new_offset;
        return 0;
    }
}

static void icelib_set_callbacks(icesnd_callback_t* cb, STREAMFILE* sf, icelib_io_t* io) {
    io->offset = 0;
    io->sf = sf;

    cb->arg = io;
    cb->read = icelib_read;
    cb->seek = icelib_seek;
}
