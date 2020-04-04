#ifndef _MUL_STREAMFILE_H_
#define _MUL_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    uint32_t block_type;
    size_t block_size;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = data->cfg.big_endian ? read_u32be : read_u32le;


    if (data->physical_offset == 0) {
        data->block_size = 0x800;
        data->data_size = 0;
        data->skip_size = 0;
        return;
    }

    block_type = read_u32(data->physical_offset + 0x00, sf);
    block_size = read_u32(data->physical_offset + 0x04, sf); /* not including main header */

    /* some blocks only contain half of data (continues in next block) so use track numbers */

    if (block_type == 0x00 && block_size != 0) {
        /* header block */
        data->block_size = 0x10;
        data->data_size = 0;
        data->skip_size = 0;
    }
    else if (block_type == 0x00000800) {
        data->block_size = 0x810;

        /* actually sub-block with size + number, kinda half-assed but meh... */
        if (block_size == data->cfg.track_number) {
            data->data_size = 0x800;
            data->skip_size = 0x10;
        }
        else{
            data->data_size = 0;
            data->skip_size = 0;
        }
    }
    else {
        /* non-audio block */
        data->block_size = block_size + 0x10;
        data->data_size = 0;
        data->skip_size = 0;
    }
}

/* Deinterleaves MUL streams */
static STREAMFILE* setup_mul_streamfile(STREAMFILE* sf, int big_endian, int track_number, int track_count) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.big_endian = big_endian;
    cfg.track_number = track_number;
    cfg.track_count = track_count;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _MUL_STREAMFILE_H_ */
