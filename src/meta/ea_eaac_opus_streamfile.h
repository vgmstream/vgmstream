#ifndef _EA_EAAC_OPUS_STREAMFILE_H_
#define _EA_EAAC_OPUS_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    data->block_size = 0x02 + read_u16be(data->physical_offset, sf);
    data->data_size = data->block_size;
}

static STREAMFILE* open_io_eaac_opus_streamfile_f(STREAMFILE *new_sf, int stream_number, int stream_count) {
    deblock_config_t cfg = {0};

    cfg.step_start = stream_number;
    cfg.step_count = stream_count;
    cfg.block_callback = block_callback;
    /* starts from 0 since new_sf is pre-deblocked */

    /* setup subfile */
    //new_sf = open_wrap_streamfile(sf); /* to be used with others */
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _EA_EAAC_OPUS_STREAMFILE_H_ */
