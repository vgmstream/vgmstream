#include "io_callback_sf.h"
#include <stdint.h>

static int64_t io_sf_read(void* dst, int size, int n, void* arg) {
    io_priv_t* io = arg;

    int bytes_read = read_streamfile(dst, io->offset, size * n, io->sf);
    int items_read = bytes_read / size;
    io->offset += bytes_read;

    return items_read;
}

static int64_t io_sf_seek(void* arg, int64_t offset, int whence) {
    io_priv_t* io = arg;

    int64_t base_offset;
    switch (whence) {
        case IO_CALLBACK_SEEK_SET:
            base_offset = 0;
            break;
        case IO_CALLBACK_SEEK_CUR:
            base_offset = io->offset;
            break;
        case IO_CALLBACK_SEEK_END:
            base_offset = get_streamfile_size(io->sf);
            break;
        default:
            return -1;
    }

    int64_t new_offset = base_offset + offset;
    if (new_offset < 0 /*|| new_offset > get_streamfile_size(config->sf)*/) {
        return -1; /* unseekable */
    }
    else {
        io->offset = new_offset;
        return 0;
    }
}

static int64_t io_sf_tell(void* arg) {
    io_priv_t* io = arg;

    return io->offset;
}

void io_callbacks_set_sf(io_callback_t* cb, io_priv_t* arg) {
    cb->arg = arg;
    cb->read = io_sf_read;
    cb->seek = io_sf_seek;
    cb->tell = io_sf_tell;
}
