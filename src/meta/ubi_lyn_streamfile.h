#ifndef _UBI_LYN_STREAMFILE_H_
#define _UBI_LYN_STREAMFILE_H_
#include "deblock_streamfile.h"

/* Deinterleaves LYN streams */
static STREAMFILE* setup_ubi_lyn_streamfile(STREAMFILE* sf, off_t stream_offset, size_t interleave_size, int stream_number, int stream_count, size_t logical_size) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.chunk_size = interleave_size;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;
    cfg.logical_size = logical_size;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "ogg");
    return new_sf;
}

#endif /* _UBI_LYN_STREAMFILE_H_ */
