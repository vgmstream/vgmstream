#ifndef _VSV_STREAMFILE_H_
#define _VSV_STREAMFILE_H_
#include "../streamfile.h"

typedef struct {
    off_t null_offset;
} vsv_io_data;

static size_t vsv_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, vsv_io_data *data) {
    int i;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    /* VSVs do start at 0x00, but first line is also the header; must null it to avoid clicks */
    if (offset < data->null_offset) {
        int max = data->null_offset - offset;
        if (max > bytes)
            max = bytes;

        for (i = 0; i < max; i++) {
            dest[i] = 0;
        }
    }
    /* VSV also has last 0x800 block with a PS-ADPCM flag of 0x10 (incorrect), but it's ignored by the decoder */

    return bytes;
}

/* cleans VSV data */
static STREAMFILE* setup_vsv_streamfile(STREAMFILE *sf) {
    STREAMFILE *new_sf = NULL;
    vsv_io_data io_data = {0};

    io_data.null_offset = 0x10;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(vsv_io_data), vsv_io_read, NULL);
    return new_sf;
}

#endif /* _VSV_STREAMFILE_H_ */
