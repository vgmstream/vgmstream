#ifndef _UBI_SB_GARBAGE_STREAMFILE_H_
#define _UBI_SB_GARBAGE_STREAMFILE_H_
#include "deblock_streamfile.h"


/* In typical Ubisoft-insane fashion, some SC:PT PS2 (not GC) streams have mixed garbage (after 0x6B00 bytes has 0x4240).
 * No apparent flag but seems to be related to stream sizes or samples (only files of +10MB, but not all).
 *
 * Since garbage is consistent between all files we can detect by checking expected crap. stream_sizes do take
 * into account extra crap, while layers don't (offset field assumes no crap), so we use a separate de-garbage
 * streamfile to simulate. */
static int is_garbage_stream(STREAMFILE* sf) {
    /* must test from file's beginning, not stream's */
    return get_streamfile_size(sf) >= 0x00800000 &&
            read_u32be(0x6B00, sf) == 0x6047BF7F &&
            read_u32be(0x6B04, sf) == 0x94FACC01;
}

//static size_t get_garbage_stream_size(off_t offset, size_t size) {
//    /* readjust size removing all possible garbage taking into account offset */
//}

static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    data->block_size = 0x6b00 + 0x4240;
    data->data_size = 0x6b00;
}

static STREAMFILE* setup_ubi_sb_garbage_streamfile_f(STREAMFILE* new_sf) {
    //STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = 0;
    cfg.block_callback = block_callback;

    /* setup sf */
    //new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _UBI_SB_GARBAGE_STREAMFILE_H_ */
