#ifndef _SSCF_ENCRYPTED_H_
#define _SSCF_ENCRYPTED_H_
#include "../streamfile.h"

typedef struct {
    uint8_t key[0x800];
} sscf_io;


static size_t sscf_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, sscf_io* io) {
    int i;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    for (i = 0; i < bytes; i++) {
        dest[i] ^= io->key[(offset + i) % sizeof(io->key)];
    }

    return bytes;
}

/* Decrypts SSCF streams */
static STREAMFILE* setup_sscf_streamfile(STREAMFILE* sf, uint32_t xorkey) {
    STREAMFILE *new_sf = NULL;
    sscf_io io = {0};
    int i;
    uint32_t stream_offset = 0x80;
    uint32_t stream_size = get_streamfile_size(sf) - stream_offset;

    /* setup key */
    xorkey = (xorkey >> 21) | (xorkey << 11);
    for (i = 0; i < sizeof(io.key); i += 4) {
        put_u32le(io.key + i, xorkey);
        xorkey += (xorkey >> 29) | (xorkey << 3);
    }

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io, sizeof(sscf_io), sscf_io_read, NULL);
    if (new_sf) {
        stream_size = read_u32le(0x84, new_sf) + 0x08; /* RIFF size */
    }
    new_sf = open_clamp_streamfile_f(new_sf, stream_offset, stream_size);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "xma");
    return new_sf;
}

#endif
