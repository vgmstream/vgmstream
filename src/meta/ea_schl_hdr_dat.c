#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

#define EA_BLOCKID_HEADER           0x5343486C /* "SCHl" */

/* EA HDR/DAT v1 (2004-2005) - used for storing speech, sometimes streamed SFX */
VGMSTREAM* init_vgmstream_ea_hdr_dat(STREAMFILE* sf) {
    VGMSTREAM* vgmstream;
    STREAMFILE* sf_dat = NULL, * temp_sf = NULL;
    int target_stream = sf->stream_index;
    uint32_t offset_mult, sound_offset, sound_size;
    uint8_t num_params, num_sounds;
    size_t dat_size;

    /* checks */
    if (!check_extensions(sf, "hdr"))
        return NULL;

    /* main header is machine endian but it's not important here */
    /* 0x00: ID */
    /* 0x02: speaker ID (used for different police voices in NFS games) */
    /* 0x04: number of parameters */
    /* 0x05: number of samples */
    /* 0x06: sample repeat (alt number of samples?) */
    /* 0x07: block size (offset multiplier) */
    /* 0x08: number of blocks (DAT size divided by block size) */
    /* 0x0a: number of sub-banks */
    /* 0x0c: table start */

    /* no nice way to validate these so we do what we can */
    if (read_u16be(0x0a, sf) != 0)
        return NULL;

    /* first offset is always zero */
    if (read_u16be(0x0c, sf) != 0)
        return NULL;

    /* must be accompanied by DAT file with SCHl or VAG sounds */
    sf_dat = open_streamfile_by_ext(sf, "dat");
    if (!sf_dat)
        goto fail;

    if (read_u32be(0x00, sf_dat) != EA_BLOCKID_HEADER &&
        read_u32be(0x00, sf_dat) != 0x56414770) /* "VAGp" */
        goto fail;

    num_params = read_u8(0x04, sf) & 0x7F;
    num_sounds = read_u8(0x05, sf);
    offset_mult = read_u8(0x07, sf) * 0x0100 + 0x0100;

    if (read_u8(0x06, sf) > num_sounds)
        goto fail;

    dat_size = get_streamfile_size(sf_dat);
    if (read_u16le(0x08, sf) * offset_mult > dat_size &&
        read_u16be(0x08, sf) * offset_mult > dat_size)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* offsets are always big endian */
    sound_offset = read_u16be(0x0C + (0x02 + num_params) * (target_stream - 1), sf) * offset_mult;
    if (read_u32be(sound_offset, sf_dat) == EA_BLOCKID_HEADER) { /* "SCHl" */
        vgmstream = load_vgmstream_ea_schl(sf_dat, sound_offset);
        if (!vgmstream)
            goto fail;
    }
    else if (read_u32be(sound_offset, sf_dat) == 0x56414770) { /* "VAGp" */
        /* Need for Speed: Hot Pursuit 2 (PS2) */
        sound_size = read_u32be(sound_offset + 0x0c, sf_dat) + 0x30;
        temp_sf = setup_subfile_streamfile(sf_dat, sound_offset, sound_size, "vag");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_vag(temp_sf);
        if (!vgmstream) goto fail;
        close_streamfile(temp_sf);
    }
    else {
        goto fail;
    }

    if (num_params != 0) {
        uint8_t val;
        char buf[8];
        int i;
        for (i = 0; i < num_params; i++) {
            val = read_u8(0x0C + (0x02 + num_params) * (target_stream - 1) + 0x02 + i, sf);
            snprintf(buf, sizeof(buf), "%u", val);
            concatn(STREAM_NAME_SIZE, vgmstream->stream_name, buf);
            if (i != num_params - 1)
                concatn(STREAM_NAME_SIZE, vgmstream->stream_name, ", ");
        }
    }

    vgmstream->num_streams = num_sounds;
    close_streamfile(sf_dat);
    return vgmstream;

fail:
    close_streamfile(sf_dat);
    close_streamfile(temp_sf);
    return NULL;
}

/* EA HDR/DAT v2 (2006-2014) */
VGMSTREAM* init_vgmstream_ea_hdr_dat_v2(STREAMFILE* sf) {
    VGMSTREAM* vgmstream;
    STREAMFILE* sf_dat = NULL;
    int target_stream = sf->stream_index;
    uint32_t offset_mult, sound_offset;
    uint8_t num_params, num_sounds;
    size_t dat_size;

    /* checks */
    if (!check_extensions(sf, "hdr"))
        return NULL;

    /* main header is machine endian but it's not important here */
    /* 0x00: ID */
    /* 0x02: number of parameters */
    /* 0x03: number of samples */
    /* 0x04: speaker ID (used for different police voices in NFS games) */
    /* 0x08: sample repeat (alt number of samples?) */
    /* 0x09: block size (offset multiplier) */
    /* 0x0a: number of blocks (DAT size divided by block size) */
    /* 0x0c: number of sub-banks (always zero?) */
    /* 0x0e: padding */
    /* 0x10: table start */

    /* no nice way to validate these so we do what we can */
    if (read_u32be(0x0c, sf) != 0)
        return NULL;

    /* first offset is always zero */
    if (read_u16be(0x10, sf) != 0)
        return NULL;

    /* must be accompanied by DAT file with SCHl sounds */
    sf_dat = open_streamfile_by_ext(sf, "dat");
    if (!sf_dat)
        goto fail;

    if (read_u32be(0x00, sf_dat) != EA_BLOCKID_HEADER)
        goto fail;

    num_params = read_u8(0x02, sf) & 0x7F;
    num_sounds = read_u8(0x03, sf);
    offset_mult = read_u8(0x09, sf) * 0x0100 + 0x0100;

    if (read_u8(0x08, sf) > num_sounds)
        goto fail;

    dat_size = get_streamfile_size(sf_dat);
    if (read_u16le(0x0a, sf) * offset_mult > dat_size &&
        read_u16be(0x0a, sf) * offset_mult > dat_size)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* offsets are always big endian */
    sound_offset = read_u16be(0x10 + (0x02 + num_params) * (target_stream - 1), sf) * offset_mult;
    if (read_u32be(sound_offset, sf_dat) != EA_BLOCKID_HEADER)
        goto fail;

    vgmstream = load_vgmstream_ea_schl(sf_dat, sound_offset);
    if (!vgmstream)
        goto fail;

    if (num_params != 0) {
        uint8_t val;
        char buf[8];
        int i;
        for (i = 0; i < num_params; i++) {
            val = read_u8(0x10 + (0x02 + num_params) * (target_stream - 1) + 0x02 + i, sf);
            snprintf(buf, sizeof(buf), "%u", val);
            concatn(STREAM_NAME_SIZE, vgmstream->stream_name, buf);
            if (i != num_params - 1)
                concatn(STREAM_NAME_SIZE, vgmstream->stream_name, ", ");
        }
    }

    vgmstream->num_streams = num_sounds;
    close_streamfile(sf_dat);
    return vgmstream;

fail:
    close_streamfile(sf_dat);
    return NULL;
}
