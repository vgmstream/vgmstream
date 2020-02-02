#ifndef _OGG_VORBIS_STREAMFILE_H_
#define _OGG_VORBIS_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    int is_encrypted;
    uint8_t key[0x100];
    size_t key_len;
    int is_nibble_swap;
    int is_header_swap;
} ogg_vorbis_io_config_data;

typedef struct {
    /* config */
    ogg_vorbis_io_config_data cfg;
} ogg_vorbis_io_data;


static size_t ogg_vorbis_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, ogg_vorbis_io_data* data) {
    static const uint8_t header_swap[4] = { 0x4F,0x67,0x67,0x53 }; /* "OggS" */
    static const size_t header_size = 0x04;
    int i;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    if (data->cfg.is_encrypted) {
        for (i = 0; i < bytes; i++) {
            if (data->cfg.is_header_swap && (offset + i) < header_size) {
                dest[i] = header_swap[(offset + i) % header_size];
            }
            else {
                if (!data->cfg.key_len && !data->cfg.is_nibble_swap)
                    break;
                if (data->cfg.key_len)
                    dest[i] ^= data->cfg.key[(offset + i) % data->cfg.key_len];
                if (data->cfg.is_nibble_swap)
                    dest[i] = ((dest[i] << 4) & 0xf0) | ((dest[i] >> 4) & 0x0f);
            }
        }
    }

    return bytes;
}

//todo maybe use generic decryption streamfile
/* Decrypts Ogg Vorbis streams */
static STREAMFILE* setup_ogg_vorbis_streamfile(STREAMFILE *sf, ogg_vorbis_io_config_data cfg) {
    STREAMFILE *new_sf = NULL;
    ogg_vorbis_io_data io_data = {0};

    io_data.cfg = cfg; /* memcpy */

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(ogg_vorbis_io_data), ogg_vorbis_io_read, NULL);
    //new_sf = open_fakename_streamfile_f(new_sf, NULL, "ogg"); //todo?
    return new_sf;
}

#endif /* _OGG_VORBIS_STREAMFILE_H_ */
