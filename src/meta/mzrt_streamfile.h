#ifndef _MZRT_STREAMFILE_H_
#define _MZRT_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    /* 0x00: samples in this block */
    data->data_size = read_s32be(data->physical_offset + 0x04, sf);
    data->skip_size = 0x08;
    data->block_size = data->skip_size + data->data_size;
}

/* Deblocks MZRT streams */
static STREAMFILE* setup_mzrt_streamfile(STREAMFILE *sf, off_t stream_offset) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.block_callback = block_callback;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    return new_sf;
}

#endif /* _MZRT_STREAMFILE_H_ */
