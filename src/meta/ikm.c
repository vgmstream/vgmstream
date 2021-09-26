#include "meta.h"
#include "../coding/coding.h"


/* IKM - MiCROViSiON PS2 container [Zwei (PS2)] */
VGMSTREAM* init_vgmstream_ikm_ps2(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!is_id32be(0x00,sf, "IKM\0"))
        goto fail;
    if (!check_extensions(sf,"ikm"))
        goto fail;

    /* 0x20: type 03? */
    if (!is_id32be(0x40,sf, "AST\0"))
        goto fail;

    loop_flag = (read_s32le(0x14, sf) > 0);
    channel_count = read_s32le(0x50, sf);
    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IKM;
    vgmstream->sample_rate = read_s32le(0x44, sf);
    vgmstream->num_samples = ps_bytes_to_samples(read_s32le(0x4c, sf), channel_count);
    vgmstream->loop_start_sample = read_s32le(0x14, sf);
    vgmstream->loop_end_sample = read_s32le(0x18, sf);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10; /* @0x40 / channels */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* IKM - MiCROViSiON PC container [Chaos Legion (PC), Legend of Galactic Heroes (PC)] */
VGMSTREAM* init_vgmstream_ikm_pc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;


    /* checks */
    if (!is_id32be(0x00,sf, "IKM\0"))
        goto fail;
    if (!check_extensions(sf,"ikm"))
        goto fail;
    /* 0x20: type 01? */

    /* find "OggS" start */
    if (is_id32be(0x30,sf, "OggS")) {
        start_offset = 0x30; /* Chaos Legion (PC) */
    }
    else if (is_id32be(0x800,sf, "OggS")) {
        start_offset = 0x800; /* Legend of Galactic Heroes (PC) */
    }
    else {
        goto fail;
    }


    {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_IKM;
        ovmi.loop_start     = read_s32le(0x14, sf);
        ovmi.loop_end       = read_s32le(0x18, sf);
        ovmi.loop_end_found = ovmi.loop_end;
        ovmi.loop_flag      = ovmi.loop_end > 0;
        ovmi.stream_size    = read_s32le(0x24, sf);

        vgmstream = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* IKM - MiCROViSiON PSP container [The Legend of Heroes: A Tear of Vermillion (PSP)] */
VGMSTREAM* init_vgmstream_ikm_psp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t start_offset;
    size_t data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "IKM\0"))
        goto fail;
    if (!check_extensions(sf,"ikm"))
        goto fail;

    /* 0x20: type 00? */
    if (!is_id32be(0x800,sf, "RIFF"))
        goto fail;

    /* loop values (pre-adjusted without encoder delay) at 0x14/18 are found in the RIFF too */
    data_size = read_s32le(0x24, sf);
    start_offset = 0x800;

    temp_sf = setup_subfile_streamfile(sf, start_offset, data_size, "at3");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_riff(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IKM;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
