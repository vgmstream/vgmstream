#ifndef _MUPS_STREAMFILE_H_
#define _MUPS_STREAMFILE_H_
#include "deblock_streamfile.h"

static inline int32_t max32(int32_t val1, int32_t val2) {
    if (val1 > val2)
        return val2;
    return val1;
}

static void read_callback(uint8_t* dst, deblock_io_data* data, size_t block_pos, size_t read_size) {
    static const uint8_t oggs[] = { 0x4F, 0x67, 0x67, 0x53 };
    static const uint8_t vorbis[] = { 0x76, 0x6F, 0x72, 0x62, 0x69, 0x73 };
    int i, min, max;

    /* Swaps Xiph magic words back (resulting page checksum is ok).
     * Reads can start/end anywhere, but block_pos = 0 is always page start */

    /* change "PssH" back to "OggS" */
    if (block_pos < 0x04) {
        min = block_pos;
        if (min < 0x00)
            min = 0x00;

        max = block_pos + read_size;
        if (max > 0x04)
            max = 0x04;

        for (i = min; i < max; i++) {
            dst[i] = oggs[i - 0x00];
        }
    }

    /* first page also needs "psolar" to "vorbis" */
    if (data->logical_offset == 0 && block_pos < 0x23) {
        min = block_pos;
        if (min < 0x1d)
            min = 0x1d;

        max = block_pos + read_size;
        if (max > 0x23)
            max = 0x23;

        for (i = min; i < max; i++) {
            dst[i] = vorbis[i - 0x1d];
        }
    }
}

static int get_page_size(STREAMFILE* sf, off_t page_offset) {
    static const int base_size = 0x1b;
    uint8_t page[0x1b + 0x100];
    uint8_t segments;
    size_t page_size;
    int i, bytes;

    bytes = read_streamfile(page + 0x00, page_offset + 0x00, base_size, sf);
    if (bytes != base_size) goto fail;

    if (get_u32be(page + 0x00) != 0x50737348) /* "PssH" */
        goto fail;

    segments = get_u8(page + 0x1a);

    bytes = read_streamfile(page + base_size, page_offset + base_size, segments, sf);
    if (bytes != segments) goto fail;

    page_size = base_size + segments;
    for (i = 0; i < segments; i++) {
        uint8_t segment_size = get_u8(page + base_size + i);
        page_size += segment_size;
    }

    return page_size;
fail:
    return -1; /* not a valid page */
}

static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    off_t page_offset = data->physical_offset;

    /* block size = OggS page size as we need read_callback called on page starts */
    data->data_size = get_page_size(sf, page_offset);
    data->block_size = data->data_size;
}

/* Fixes MUPS streams that contain mutated OggS */
static STREAMFILE* setup_mups_streamfile(STREAMFILE* sf, off_t stream_offset) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.block_callback = block_callback;
    cfg.read_callback = read_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "ogg");
    return new_sf;
}

#endif /* _MUPS_STREAMFILE_H_ */
