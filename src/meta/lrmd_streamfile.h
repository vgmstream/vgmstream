#ifndef _LRMD_STREAMFILE_H_
#define _LRMD_STREAMFILE_H_
#include "deblock_streamfile.h"

/* Deinterleaves LRMD streams */
static STREAMFILE* setup_lrmd_streamfile(STREAMFILE *sf, size_t interleave_size, int stream_number, int stream_count) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.chunk_size = interleave_size;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _LRMD_STREAMFILE_H_ */
