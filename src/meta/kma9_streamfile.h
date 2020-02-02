#ifndef _KM9_STREAMFILE_H_
#define _KM9_STREAMFILE_H_
#include "deblock_streamfile.h"

/* Deinterleaves KMA9 streams */
static STREAMFILE* setup_kma9_streamfile(STREAMFILE *sf, off_t stream_offset, size_t stream_size, size_t interleave_size, int stream_number, int stream_count) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.logical_size = stream_size;
    cfg.chunk_size = interleave_size;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _KM9_STREAMFILE_H_ */
