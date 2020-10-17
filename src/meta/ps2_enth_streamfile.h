#ifndef _LP_STREAMFILE_H_
#define _LP_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    off_t start;
} lp_io_data;

static size_t lp_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, lp_io_data* data) {
    int i;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    /* PCM16LE frames are ROL'ed (or lower bit is ignored?) so this only works if reads are aligned
     * to sizeof(uint16_t); maybe could be a new decoder but seems like a waste */
    for (i = 0; i < bytes / 2 * 2; i += 2) {
        if (offset + i >= data->start) {
            uint16_t v = get_u16le(dest + i);
            v = (v << 1) | ((v >> 15) & 0x0001);
            put_u16le(dest + i, v);
        }
    }

    return bytes;
}

/* decrypts Enthusia "LP" PCM streams */
static STREAMFILE* setup_lp_streamfile(STREAMFILE* sf, off_t start) {
    STREAMFILE* new_sf = NULL;
    lp_io_data io_data = {0};

    io_data.start = start;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(lp_io_data), lp_io_read, NULL);
    return new_sf;
}

#endif /* _LP_STREAMFILE_H_ */
