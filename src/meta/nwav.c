#include "meta.h"
#include "../coding/coding.h"

/* NWAV - from Chunsoft games [Fuurai no Shiren Gaiden: Onnakenshi Asuka Kenzan! (PC)] */
VGMSTREAM* init_vgmstream_nwav(STREAMFILE* sf) {
    off_t start_offset;


    /* checks */
    if (!is_id32be(0x00,sf, "NWAV"))
        goto fail;
    /* .nwav: header id (no filenames in bigfiles) */
    if (!check_extensions(sf,"nwav,") )
        goto fail;


    {
        ogg_vorbis_meta_info_t ovmi = {0};
        int channels;

        /* 0x04: version? */
        /* 0x08: crc? */
        ovmi.stream_size = read_u32le(0x0c, sf);
        ovmi.loop_end = read_u32le(0x10, sf); /* num_samples, actually */
        /* 0x14: sample rate */
        /* 0x18: bps? (16) */
        channels = read_u8(0x19, sf);
        start_offset = read_u16le(0x1a, sf);

        ovmi.loop_flag = read_u16le(0x1c, sf) != 0; /* loop count? -1 = loops */
        /* 0x1e: always 2? */
        /* 0x20: always 1? */
        ovmi.loop_start = read_u32le(0x24, sf);
        /* 0x28: always 1? */
        /* 0x2a: always 1? */
        /* 0x2c: always null? */

        ovmi.meta_type = meta_NWAV;

        /* values are in resulting bytes */
        ovmi.loop_start = ovmi.loop_start / sizeof(int16_t) / channels;
        ovmi.loop_end = ovmi.loop_end / sizeof(int16_t) / channels;

        return init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
    }

fail:
    return NULL;
}
