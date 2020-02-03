#ifndef _SFH_STREAMFILE_H_
#define _SFH_STREAMFILE_H_
#include "deblock_streamfile.h"

/* Deblocks SFH streams, skipping 0x10 garbage added to every chunk */
static STREAMFILE* setup_sfh_streamfile(STREAMFILE *sf, off_t stream_offset, size_t chunk_size, size_t clean_size, const char* extension) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.chunk_size = chunk_size;
    cfg.skip_size = 0x10;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    new_sf = open_clamp_streamfile_f(new_sf, 0x00, clean_size);
    if (extension)
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    return new_sf;
}

#endif /* _SFH_STREAMFILE_H_ */
