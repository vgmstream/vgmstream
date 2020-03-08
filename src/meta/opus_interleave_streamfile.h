#ifndef _OPUS_INTERLEAVE_STREAMFILE_H_
#define _OPUS_INTERLEAVE_STREAMFILE_H_
#include "deblock_streamfile.h"


static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    uint32_t chunk_size = read_u32be(data->physical_offset, sf);
    
    //todo not ok if offset between 0 and header_size
    if (chunk_size == 0x01000080) /* header */
        chunk_size = read_u32le(data->physical_offset + 0x10, sf) + 0x08;
    else
        chunk_size += 0x08;
    data->block_size = chunk_size;
    data->data_size = data->block_size;
}

/* Deblocks NXOPUS streams that interleave 1 packet per stream */
static STREAMFILE* setup_opus_interleave_streamfile(STREAMFILE* sf, off_t start_offset, int stream_number, int stream_count) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = start_offset;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;
    {
        int i;
        off_t offset = start_offset;
        /* read full size from NXOPUS header N */
        for (i = 0; i < stream_number + 1; i++) {
            off_t start = read_s32le(offset + 0x10, sf);
            off_t size  = read_s32le(offset + start + 0x04, sf);

            if (i == stream_number)
                cfg.logical_size = 0x08 + start + size;
            offset += 0x08 + start;
        }
    }
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    return new_sf;
}

#endif /* _OPUS_INTERLEAVE_STREAMFILE_H_ */
