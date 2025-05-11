#ifndef _LRMD_STREAMFILE_H_
#define _LRMD_STREAMFILE_H_
#include "deblock_streamfile.h"

static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    data->block_size = 0x10000;
    data->data_size = data->block_size - 0x10;
}

/* Deinterleaves .AUDIOPKG Xbox streams */
static STREAMFILE* setup_audiopkg_streamfile(STREAMFILE* sf, uint32_t stream_offset, uint32_t stream_size, int stream_number, int stream_count) {
    STREAMFILE* new_sf = NULL;

    // blocks: [B1-0x8000-ch1][B1-0x8000-ch2][B2-0x7FF0-ch1 + 0x10 padding][B2-0x7FF0-ch2 + 0x10 padding] xN
    // For now add a deblock of L/R blocks, then another to remove padding.
    // Probably should mix into a single deblocker but can't think of anything decent at the moment.

    deblock_config_t cfg1 = {0};
    cfg1.stream_start = stream_offset;
    cfg1.stream_size = stream_size;
    cfg1.step_start = stream_number;
    cfg1.step_count = stream_count;
    cfg1.chunk_size = 0x8000;

    deblock_config_t cfg2 = {0};
    cfg2.block_callback = block_callback;

    //TODO: this SF is handled a bit differently than usual, improve
    new_sf = reopen_streamfile(sf, 0); //open_wrap_streamfile(sf); 
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg1);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg2);
    return new_sf;
}

#endif
