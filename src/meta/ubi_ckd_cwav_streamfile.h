#ifndef _UBI_CKD_CWAV_STREAMFILE_H_
#define _UBI_CKD_CWAV_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    uint32_t chunk_type = read_u32be(data->physical_offset + 0x00, sf);
    uint32_t chunk_size = read_u32le(data->physical_offset + 0x04, sf);

    if (chunk_type == get_id32be("RIFF")) {
        data->data_size = 0x0;
        data->skip_size = 0x0;
        data->block_size = 0x0c;
    }
    else {
        data->data_size = chunk_size;
        data->skip_size = 0x08;
        data->block_size = data->data_size + data->skip_size;
    }
}

/* Deblocks CWAV streams inside RIFF */
static STREAMFILE* setup_ubi_ckd_cwav_streamfile(STREAMFILE* sf) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = 0x00;
    cfg.stream_size = get_streamfile_size(sf);
    cfg.block_callback = block_callback;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "bcwav");
    return new_sf;
}

#endif /* _UBI_CKD_CWAV_STREAMFILE_H_ */
