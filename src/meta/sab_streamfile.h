#ifndef _SAB_STREAMFILE_H_
#define _SAB_STREAMFILE_H_
#include "deblock_streamfile.h"

static STREAMFILE* setup_sab_streamfile(STREAMFILE* sf, off_t stream_start, int stream_count, int stream_number, size_t interleave) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_start;
    cfg.chunk_size = interleave;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _SAB_STREAMFILE_H_ */
