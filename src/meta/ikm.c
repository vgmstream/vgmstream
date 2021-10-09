#include "meta.h"
#include "../coding/coding.h"


static VGMSTREAM* init_vgmstream_ikm_ps2(STREAMFILE* sf) {
    VGMSTREAM* v = NULL;
    off_t start_offset;
    int loop_flag, channels;

    if (!is_id32be(0x40,sf, "AST\0"))
        goto fail;

    loop_flag = (read_s32le(0x14, sf) > 0);
    channels = read_s32le(0x50, sf);
    start_offset = 0x800;


    /* build the VGMSTREAM */
    v = allocate_vgmstream(channels, loop_flag);
    if (!v) goto fail;

    v->meta_type = meta_IKM;
    v->sample_rate = read_s32le(0x44, sf);
    v->num_samples = ps_bytes_to_samples(read_s32le(0x4c, sf), channels);
    v->loop_start_sample = read_s32le(0x14, sf);
    v->loop_end_sample = read_s32le(0x18, sf);
    v->coding_type = coding_PSX;
    v->layout_type = layout_interleave;
    v->interleave_block_size = 0x10; /* @0x40 / channels */

    if (!vgmstream_open_stream(v, sf, start_offset))
        goto fail;
    return v;

fail:
    close_vgmstream(v);
    return NULL;
}

static VGMSTREAM* init_vgmstream_ikm_pc(STREAMFILE* sf) {
    VGMSTREAM* v = NULL;
    off_t start_offset;

    /* find "OggS" start */
    if (is_id32be(0x30,sf, "OggS"))
        start_offset = 0x30; /* Chaos Legion (PC) */
    else
        start_offset = 0x800; /* Legend of Galactic Heroes (PC) */

    {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_IKM;
        ovmi.loop_start     = read_s32le(0x14, sf);
        ovmi.loop_end       = read_s32le(0x18, sf);
        ovmi.loop_end_found = ovmi.loop_end;
        ovmi.loop_flag      = ovmi.loop_end > 0;
        ovmi.stream_size    = read_s32le(0x24, sf);

        v = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
        if (!v) goto fail;
    }

    return v;

fail:
    close_vgmstream(v);
    return NULL;
}

static VGMSTREAM* init_vgmstream_ikm_psp(STREAMFILE* sf) {
    VGMSTREAM* v = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t start_offset;
    size_t data_size;

    if (!is_id32be(0x800,sf, "RIFF"))
        goto fail;

    /* loop values (pre-adjusted without encoder delay) at 0x14/18 are found in the RIFF too */
    data_size = read_s32le(0x24, sf);
    start_offset = 0x800;

    temp_sf = setup_subfile_streamfile(sf, start_offset, data_size, "at3");
    if (!temp_sf) goto fail;

    v = init_vgmstream_riff(temp_sf);
    if (!v) goto fail;

    v->meta_type = meta_IKM;

    close_streamfile(temp_sf);
    return v;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(v);
    return NULL;
}

/* IKM - MiCROViSiON container */
VGMSTREAM* init_vgmstream_ikm(STREAMFILE* sf) {
    uint32_t type;

    /* checks */
    if (!is_id32be(0x00,sf, "IKM\0"))
        goto fail;

    if (!check_extensions(sf,"ikm"))
        goto fail;

    type = read_u32le(0x20, sf);
    switch(type) {
        case 0x00: /* The Legend of Heroes: A Tear of Vermillion (PSP) */
            return init_vgmstream_ikm_psp(sf);
        case 0x01: /* Chaos Legion (PC), Legend of Galactic Heroes (PC) */
            return init_vgmstream_ikm_pc(sf);
        case 0x03: /* Zwei (PS2) */
            return init_vgmstream_ikm_ps2(sf);
        default:
            goto fail;
    }

fail:
    return NULL;
}
