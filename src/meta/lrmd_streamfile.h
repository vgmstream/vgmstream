#ifndef _LRMD_STREAMFILE_H_
#define _LRMD_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    data->data_size = data->cfg.frame_size;
    data->skip_size = data->cfg.skip_size;
    data->block_size = data->cfg.chunk_size;
}

/* Deinterleaves LRMD streams */
static STREAMFILE* setup_lrmd_streamfile(STREAMFILE *sf, size_t block_size, size_t chunk_start, size_t chunk_size) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.frame_size = block_size;
    cfg.chunk_size = chunk_size;
    cfg.skip_size  = chunk_start;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _LRMD_STREAMFILE_H_ */
