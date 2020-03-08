#ifndef _XWMA_KONAMI_STREAMFILE_H_
#define _XWMA_KONAMI_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    data->block_size = align_size_to_block(data->cfg.chunk_size, 0x10);
    data->data_size = data->cfg.chunk_size;
    data->skip_size = 0x00;
}

/* De-pads Konami XWMA streams */
static STREAMFILE* setup_xwma_konami_streamfile(STREAMFILE* sf, off_t stream_offset, size_t block_align) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.chunk_size = block_align;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    return new_sf;
}

#endif /* _XWMA_KONAMI_STREAMFILE_H_ */
