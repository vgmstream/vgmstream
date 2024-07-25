#ifndef _FSB5_STREAMFILE_H_
#define _FSB5_STREAMFILE_H_
#include "deblock_streamfile.h"

static STREAMFILE* setup_fsb5_streamfile(STREAMFILE* sf, off_t stream_start, size_t stream_size, int stream_count, int stream_number, size_t interleave) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_start;
    cfg.stream_size = stream_size;
    cfg.chunk_size = interleave;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _FSB5_STREAMFILE_H_ */
