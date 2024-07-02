#include "meta.h"
#include "../util/endianness.h"


#define EAAC_BLOCKID0_DATA              0x00
#define EAAC_BLOCKID0_END               0x80 /* maybe meant to be a bitflag? */


/* EA HDR/STH/DAT - seen in older 7th gen games, used for storing speech */
VGMSTREAM* init_vgmstream_ea_hdr_sth_dat(STREAMFILE* sf) {
    VGMSTREAM* vgmstream;
    STREAMFILE *sf_dat = NULL, *sf_sth = NULL;
    int target_stream = sf->stream_index;
    uint32_t snr_offset, sns_offset, block_size;
    uint16_t sth_offset, sth_offset2;
    uint8_t num_params, num_sounds, block_id;
    size_t dat_size;
    read_u32_t read_u32;
    eaac_meta_t info = {0};

    /* 0x00: ID */
    /* 0x02: number of parameters */
    /* 0x03: number of samples */
    /* 0x04: speaker ID (used for different police voices in NFS games) */
    /* 0x08: sample repeat (alt number of samples?) */
    /* 0x09: block size (always zero?) */
    /* 0x0a: number of blocks (related to size?) */
    /* 0x0c: number of sub-banks (always zero?) */
    /* 0x0e: padding */
    /* 0x10: table start */

    if (!check_extensions(sf, "hdr"))
        return NULL;

    if (read_u8(0x09, sf) != 0)
        return NULL;

    if (read_u32be(0x0c, sf) != 0)
        return NULL;

    /* first offset is always zero */
    if (read_u16be(0x10, sf) != 0)
        return NULL;

    sf_sth = open_streamfile_by_ext(sf, "sth");
    if (!sf_sth) goto fail;

    sf_dat = open_streamfile_by_ext(sf, "dat");
    if (!sf_dat) goto fail;

    /* STH always starts with the first offset of zero */
    sns_offset = read_u32be(0x00, sf_sth);
    if (sns_offset != 0)
        goto fail;

    /* check if DAT starts with a correct SNS block */
    block_id = read_u8(0x00, sf_dat);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    num_params = read_u8(0x02, sf) & 0x7F;
    num_sounds = read_u8(0x03, sf);

    if (read_u8(0x08, sf) > num_sounds)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* offsets in HDR are always big endian */
    sth_offset = read_u16be(0x10 + (0x02 + num_params) * (target_stream - 1), sf);

#if 0
    snr_offset = sth_offset + 0x04;
    sns_offset = read_u32(sth_offset + 0x00, sf_sth);
#else
    /* overly intricate way to detect byte endianness because of the simplicity of HDR format */
    dat_size = get_streamfile_size(sf_dat);
    snr_offset = 0;
    sns_offset = 0;

    if (num_sounds == 1) {
        /* always 0 */
        snr_offset = sth_offset + 0x04;
        sns_offset = 0x00;
    }
    else {
        /* find the first sound size and match it up with the second sound offset to detect endianness */
        while (1) {
            if (sns_offset >= dat_size)
                goto fail;

            block_id = read_u8(sns_offset, sf_dat);
            block_size = read_u32be(sns_offset, sf_dat) & 0x00FFFFFF;
            if (block_size == 0)
                goto fail;

            if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
                goto fail;

            sns_offset += block_size;

            if (block_id == EAAC_BLOCKID0_END)
                break;
        }

        sns_offset = align_size_to_block(sns_offset, 0x40);
        sth_offset2 = read_u16be(0x10 + (0x02 + num_params) * 1, sf);
        if (sns_offset == read_u32be(sth_offset2, sf_sth)) {
            read_u32 = read_u32be;
        }
        else if (sns_offset == read_u32le(sth_offset2, sf_sth)) {
            read_u32 = read_u32le;
        }
        else {
            goto fail;
        }

        snr_offset = sth_offset + 0x04;
        sns_offset = read_u32(sth_offset + 0x00, sf_sth);
    }
#endif

    block_id = read_u8(sns_offset, sf_dat);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    info.sf_head = sf_sth;
    info.sf_body = sf_dat;
    info.head_offset = snr_offset;
    info.body_offset = sns_offset;
    info.type = meta_EA_SNR_SNS;

    vgmstream = load_vgmstream_ea_eaac(&info);
    if (!vgmstream) goto fail;

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
    close_streamfile(sf_sth);
    close_streamfile(sf_dat);
    return vgmstream;

fail:
    close_streamfile(sf_sth);
    close_streamfile(sf_dat);
    return NULL;
}
