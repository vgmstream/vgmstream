#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

static VGMSTREAM* init_vgmstream_kt_wiibgm_offset(STREAMFILE* sf, off_t offset);

/* Koei Tecmo G1L - container format, sometimes containing a single stream.
 * It probably makes more sense to extract it externally, it's here mainly for Hyrule Warriors */
VGMSTREAM* init_vgmstream_kt_g1l(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int type;
    int total_streams, target_stream = sf->stream_index;
    off_t stream_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "G1L_") && /* BE */
        !is_id32le(0x00, sf, "G1L_"))   /* LE */
        goto fail;

    if (!check_extensions(sf,"g1l"))
        goto fail;

    if (!is_id32be(0x04, sf, "0000")) /* version? */
        goto fail;

    if (is_id32be(0x00, sf, "G1L_")) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }


    /* 0x08: filesize, 0x0c: header size */
    type = read_32bit(0x10,sf);
    total_streams = read_32bit(0x14,sf);
    if (target_stream==0) target_stream = 1;
    if (target_stream < 0 || target_stream > total_streams || total_streams < 1) goto fail;

    stream_offset = read_32bit(0x18 + 0x4*(target_stream-1),sf);
    //stream_size = stream_offset - stream_next_offset;//not ok, sometimes entries are unordered/repeats */

    switch(type) { /* type may not be correct */
        case 0x09: /* DSP (WiiBGM) from Hyrule Warriors (Wii U) */
            vgmstream = init_vgmstream_kt_wiibgm_offset(sf, stream_offset);
            break;
        case 0x06: /* ATRAC9 (RIFF) from One Piece Pirate Warriors 3 (Vita) */
        case 0x01: /* ATRAC3plus (RIFF) from One Piece Pirate Warriors 2 (PS3) */
        case 0x00: /* OGG (KOVS) from Romance Three Kindgoms 13 (PC)*/
        case 0x0A: /* OGG (KOVS) from Dragon Quest Heroes (PC), Bladestorm (PC) w/ single files */
        default:
            goto fail;
    }


    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Koei Tecmo "WiiBGM" DSP format - found in Hyrule Warriors, Romance of the Three Kingdoms 12 */
VGMSTREAM * init_vgmstream_kt_wiibgm(STREAMFILE *sf) {
    return init_vgmstream_kt_wiibgm_offset(sf, 0x0);
}

static VGMSTREAM* init_vgmstream_kt_wiibgm_offset(STREAMFILE* sf, off_t offset) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channel_count;
    off_t start_offset;

    /* check */
    if (!is_id64be(offset+0x0, sf, "WiiBGM\0\0") &&
        read_32bitBE(offset+0x4, sf) != 0x474D0000)
        goto fail;

    if (!check_extensions(sf,"g1l,dsp"))
        goto fail;

    /* check type details */
    loop_flag = read_32bitBE(offset+0x14, sf) > 0;
    channel_count = read_u8(offset+0x23, sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = read_32bitBE(offset+0x10, sf);
    vgmstream->sample_rate = (uint16_t)read_16bitBE(offset+0x26, sf);
    vgmstream->loop_start_sample = read_32bitBE(offset+0x14, sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP_subint;
    vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = 0x1;
    vgmstream->meta_type = meta_KT_WIIBGM;

    dsp_read_coefs_be(vgmstream,sf, offset+0x5C, 0x60);
    start_offset = offset+0x800;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
