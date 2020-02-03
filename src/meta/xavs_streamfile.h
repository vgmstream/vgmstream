#ifndef _XAVS_STREAMFILE_H_
#define _XAVS_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    uint32_t chunk_type = read_u32le(data->physical_offset, sf) & 0xFF;
    uint32_t chunk_size = read_u32le(data->physical_offset, sf) >> 8;

    data->skip_size = 0x04;

    switch(chunk_type) {
        /* audio */
        case 0x41:
        case 0x61:
        case 0x62:
        case 0x63:
            data->block_size = 0x04 + chunk_size;
            if (data->cfg.track_number + 1 == (chunk_type & 0x0F)) {
                data->data_size = chunk_size;
            } else {
                data->data_size = 0; /* ignore other subsongs */
            }
            break;

        /* video */
        case 0x56:
            data->block_size = 0x04 + chunk_size;
            data->data_size = 0;
            break;

        /* empty */
        case 0x21: /* related to video */
        case 0x5F: /* "_EOS" */
            data->block_size = 0x04;
            data->data_size = 0;
            break;

        default:
            VGM_LOG("XAVS: unknown type at %lx\n", data->physical_offset);
            data->block_size = 0x04;
            data->data_size = 0;
            break;
    }
}

/* Deblocks XAVS video/audio data */
static STREAMFILE* setup_xavs_streamfile(STREAMFILE *sf, off_t stream_offset, int stream_number) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.track_number = stream_number;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile(new_sf,0);
    return new_sf;
}

#endif /* _XAVS_STREAMFILE_H_ */
