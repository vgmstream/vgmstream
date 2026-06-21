#ifndef _VIG_KCES_STREAMFILE_H_
#define _VIG_KCES_STREAMFILE_H_
#include "../streamfile.h"
#include "../util/reader_sf.h"


typedef struct {
    uint8_t xor;
    uint8_t add;
    off_t start_offset;
    size_t data_size;
} vig_kces_decryption_data;

static size_t vig_kces_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, vig_kces_decryption_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (xor) */
    for (i = 0; i < bytes_read; i++) {
        if (offset+i >= data->start_offset && offset+i < data->start_offset + data->data_size) {
            if (((offset+i) % 0x10) == 0) /* XOR header byte per frame */
                dest[i] = dest[i] ^ data->xor;
            else if (((offset+i) % 0x10) == 2) /* ADD first data byte per frame */
                dest[i] = (uint8_t)(dest[i] + data->add);
        }
    }

    return bytes_read;
}

static STREAMFILE* setup_vig_kces_streamfile(STREAMFILE* sf, off_t start_offset, size_t data_size) {
    STREAMFILE* new_sf = NULL;
    vig_kces_decryption_data io_data = {0};
    size_t io_data_size = sizeof(vig_kces_decryption_data);

    /* setup decryption (usually xor=0xFF and add=0x02) */
    io_data.xor = read_u8(start_offset + 0x00, sf);
    io_data.add = (~read_u8(start_offset + 0x02, sf)) + 0x01;
    io_data.start_offset = start_offset;
    io_data.data_size = data_size;


    /* setup custom streamfile */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data,io_data_size, vig_kces_decryption_read,NULL);
    return new_sf;
}

#endif
