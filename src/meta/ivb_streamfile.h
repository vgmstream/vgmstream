#ifndef _STR_WAV_STREAMFILE_H_
#define _STR_WAV_STREAMFILE_H_
#include "deblock_streamfile.h"

/* Deblocks streams */
static STREAMFILE* setup_ivb_streamfile(STREAMFILE* sf, uint32_t stream_start, int stream_count, int stream_number, uint32_t chunk_size) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_start;
    cfg.chunk_size = chunk_size;
    cfg.step_start = stream_number;
    cfg.step_count = stream_count;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif
