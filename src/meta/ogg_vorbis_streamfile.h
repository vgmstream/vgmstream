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


static size_t ogg_vorbis_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, ogg_vorbis_io_data* data) {
    size_t bytes_read;
    int i;
    static const uint8_t header_swap[4] = { 0x4F,0x67,0x67,0x53 }; /* "OggS" */
    static const size_t header_size = 0x04;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    if (data->cfg.is_encrypted) {
        for (i = 0; i < bytes_read; i++) {
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

    return bytes_read;
}

//todo maybe use generic decryption streamfile
static STREAMFILE* setup_ogg_vorbis_streamfile(STREAMFILE *streamFile, ogg_vorbis_io_config_data cfg) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    ogg_vorbis_io_data io_data = {0};
    size_t io_data_size = sizeof(ogg_vorbis_io_data);

    /* setup decryption */
    io_data.cfg = cfg; /* memcpy */


    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    //todo extension .ogg?

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, ogg_vorbis_io_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}


#endif /* _OGG_VORBIS_STREAMFILE_H_ */
