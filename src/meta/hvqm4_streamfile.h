#ifndef _HVQM4_STREAMFILE_H_
#define _HVQM4_STREAMFILE_H_
#include "deblock_streamfile.h"
#include "../util/reader_sf.h"

static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    uint32_t block_offset = data->physical_offset;

    /* detect new block, using chunk_size as counter (a bit hacky but whatevs) */
    if (data->chunk_size <= 0) {
        // 0x00: last_full_block_size (slightly smaller in .afc)
        uint32_t full_block_size      = read_u32be(block_offset+0x04, sf);
        /* 0x08: vid_frame_count */
        /* 0x0c: aud_frame_count */
        /* 0x10: flags + padding (0x01000000, except 0 in a couple of Bomberman Jetters files) */

        data->chunk_size = full_block_size; // not including 0x14 block header
        data->block_size = 0x14;
        data->data_size = 0; // signal new block
        return;
    }
    
    /* new audio or video frames in the current block */
    uint16_t frame_type     = read_u16be(block_offset+0x00, sf);
    uint16_t frame_format   = read_u16be(block_offset+0x02, sf);
    uint32_t frame_size     = read_u32be(block_offset+0x04, sf); // not including 0x08 frame header

    if (frame_type == 0x00 && frame_format == 0xFF00) {
        // AFC [Pikmin (PC)]
        data->skip_size = 0x08;
        data->data_size = frame_size;
        data->block_size = data->skip_size + data->data_size;
        
#if 0
        /* skip data from other audio tracks: not seen in AFC */
        if (cfg.track_number ...) {
            uint32_t track_skip = ...;
            data->skip_size += track_skip;
            data->data_size -= track_skip;
        }
#endif
    }
    else {
        // IMA audio and video frames
        data->block_size = 0x08 + frame_size;
        data->skip_size = 0;
        data->data_size = 0;
    }

    data->chunk_size -= data->block_size;
}

/* Demuxes HVQM4 streams. Mainly for AFC data, which is split between blocks in the middle of frames. */
static STREAMFILE* setup_hvqm4_streamfile(STREAMFILE* sf, uint32_t stream_offset, int stream_number, int stream_count) {
    STREAMFILE* new_sf = NULL;

    deblock_config_t cfg = {0};
    cfg.stream_start = stream_offset;
    cfg.track_number = stream_number;
    cfg.track_count = stream_count;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif
