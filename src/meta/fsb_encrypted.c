#include "meta.h"
#include "fsb_keys.h"

#define FSB_KEY_MAX 128 /* probably 32 */

static STREAMFILE* setup_fsb_streamfile(STREAMFILE *streamFile, const uint8_t * key, size_t key_size, int is_alt);


/* fully encrypted FSBs */
VGMSTREAM * init_vgmstream_fsb_encrypted(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;

    /* checks */
    /* .fsb: standard
     * .fsb.xen: various Guitar Hero (X360) */
    if ( !check_extensions(streamFile, "fsb,xen") )
        goto fail;

    /* ignore non-encrypted FSB */
    if ((read_32bitBE(0x00,streamFile) & 0xFFFFFF00) == 0x46534200) /* "FSB\0" */
        goto fail;


    /* try fsbkey + all combinations of FSB4/5 and decryption algorithms */
    {
        STREAMFILE *temp_streamFile = NULL;
        uint8_t key[FSB_KEY_MAX];
        size_t key_size = read_key_file(key, FSB_KEY_MAX, streamFile);

        if (key_size) {
            {
                temp_streamFile = setup_fsb_streamfile(streamFile, key,key_size, 0);
                if (!temp_streamFile) goto fail;

                if (!vgmstream) vgmstream = init_vgmstream_fsb(temp_streamFile);
                if (!vgmstream) vgmstream = init_vgmstream_fsb5(temp_streamFile);

                close_streamfile(temp_streamFile);
            }

            if (!vgmstream) {
                temp_streamFile = setup_fsb_streamfile(streamFile, key,key_size, 1);
                if (!temp_streamFile) goto fail;

                if (!vgmstream) vgmstream = init_vgmstream_fsb(temp_streamFile);
                if (!vgmstream) vgmstream = init_vgmstream_fsb5(temp_streamFile);

                close_streamfile(temp_streamFile);
            }
        }
    }


    /* try all keys until one works */
    if (!vgmstream) {
        int i;
        STREAMFILE *temp_streamFile = NULL;

        for (i = 0; i < fsbkey_list_count; i++) {
            fsbkey_info entry = fsbkey_list[i];
            //;VGM_LOG("fsbkey: size=%i, is_fsb5=%i, is_alt=%i\n", entry.fsbkey_size,entry.is_fsb5, entry.is_alt);

            temp_streamFile = setup_fsb_streamfile(streamFile, entry.fsbkey, entry.fsbkey_size, entry.is_alt);
            if (!temp_streamFile) goto fail;

            if (fsbkey_list[i].is_fsb5) {
                vgmstream = init_vgmstream_fsb5(temp_streamFile);
            } else {
                vgmstream = init_vgmstream_fsb(temp_streamFile);
            }

            close_streamfile(temp_streamFile);
            if (vgmstream) break;
        }
    }

    if (!vgmstream)
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


typedef struct {
    uint8_t key[FSB_KEY_MAX];
    size_t key_size;
    int is_alt;
} fsb_decryption_data;

/* Encrypted FSB info from guessfsb and fsbext */
static size_t fsb_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, fsb_decryption_data* data) {
    static const unsigned char reverse_bits_table[] = { /* LUT to simplify, could use some bitswap function */
      0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
      0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
      0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
      0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
      0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
      0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
      0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
      0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
      0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
      0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
      0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
      0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
      0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
      0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
      0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
      0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
    };
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (inverted bits and xor) */
    for (i = 0; i < bytes_read; i++) {
        uint8_t xor = data->key[(offset + i) % data->key_size];
        uint8_t val = dest[i];
        if (data->is_alt) {
            dest[i] = reverse_bits_table[val ^ xor];
        }
        else {
            dest[i] = reverse_bits_table[val] ^ xor;
        }
    }

    return bytes_read;
}

static STREAMFILE* setup_fsb_streamfile(STREAMFILE *streamFile, const uint8_t * key, size_t key_size, int is_alt) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    fsb_decryption_data io_data = {0};
    size_t io_data_size = sizeof(fsb_decryption_data);

    /* setup decryption with key (external) */
    if (!key_size || key_size > FSB_KEY_MAX) goto fail;

    memcpy(io_data.key, key, key_size);
    io_data.key_size = key_size;
    io_data.is_alt = is_alt;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, fsb_decryption_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,"fsb");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
