#ifndef _EA_SCHL_STREAMFILE_H_
#define _EA_SCHL_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    uint32_t block_type, block_size;

    block_type = read_u32be(data->physical_offset + 0x00, sf);
    block_size = read_u32le(data->physical_offset + 0x04, sf); /* always LE, hopefully */

    if (block_type == 0x5343456C) /* "SCEl" end block  */
        return;

    data->block_size = block_size;
    if (block_type != 0x5343446C) /* skip non-"SCDl" blocks */
        return;

    switch(data->cfg.codec) {
        case 0x1a: /* ATRAC3 */
        case 0x1b: /* ATRAC3plus */
            data->data_size = read_32bitLE(data->physical_offset + 0x0c + 0x04 * data->cfg.channels, sf);
            data->skip_size = 0x0c + 0x04 * data->cfg.channels + 0x04;
            break;
        default:
            break;
    }
}

/* Deblocks SCHl streams */
static STREAMFILE* setup_schl_streamfile(STREAMFILE *sf, int codec, int channels, off_t stream_offset, size_t logical_size) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.logical_size = logical_size;
    cfg.codec = codec;
    cfg.channels = channels; //todo chunk size?
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile(new_sf, 0);
    return new_sf;
}

#endif /* _EA_SCHL_STREAMFILE_H_ */
