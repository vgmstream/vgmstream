#include "meta.h"
#include "../layout/layout.h"

/* .SNU - EA new-ish header (Dead Space, The Godfather 2) */
VGMSTREAM * init_vgmstream_ea_snu(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag = 0, channel_config, codec, sample_rate, flags;
    uint32_t num_samples, loop_start = 0, loop_end = 0;
    off_t start_offset;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"snu"))
        goto fail;

    /* check header */
    //if ((read_32bitBE(0x00,streamFile) & 0x00FFFF00 != 0x00000000) && (read_32bitBE(0x0c,streamFile) != 0x00000000))
    //    goto fail;
    /* 0x00: related to sample rate?, 0x02: always 0?, 0x03: related to channels? (usually match but may be 0) */
    /* 0x04: some size, maybe >>2 ~= number of 0x4c frames (BE/LE depending on platform) */
    /* 0x08: always 0x20? (also BE/LE), 0x0c: always 0? */


    start_offset = 0x20; /* first block */

    codec = read_8bit(0x10,streamFile);
    channel_config = read_8bit(0x11,streamFile);
    sample_rate = (uint16_t)read_16bitBE(0x12,streamFile);
    flags = (uint8_t)read_8bit(0x14,streamFile); /* upper nibble only? */
    num_samples = (uint32_t)read_32bitBE(0x14,streamFile) & 0x00FFFFFF;
    /* 0x18: null?, 0x1c: null? */

    if (flags != 0x60 && flags != 0x40) {
        VGM_LOG("EA SNS: unknown flag\n");
        goto fail;
    }

#if 0
    //todo not working ok with blocks
    if (flags & 0x60) { /* full loop, seen in ambient tracks */
        loop_flag = 1;
        loop_start = 0;
        loop_end = num_samples;
    }
#endif

    //channel_count = (channel_config >> 2) + 1; //todo test
    /* 01/02/03 = 1 ch?, 05/06/07 = 2/3 ch?, 0d/0e/0f = 4/5 ch?, 14/15/16/17 = 6/7 ch?, 1d/1e/1f = 8 ch? */
    switch(channel_config) {
        case 0x00: channel_count = 1; break;
        case 0x04: channel_count = 2; break;
        case 0x0c: channel_count = 4; break;
        case 0x14: channel_count = 6; break;
        case 0x1c: channel_count = 8; break;
        default:
            VGM_LOG("EA SNU: unknown channel config 0x%02x\n", channel_config);
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_EA_SNU;
    vgmstream->layout_type = layout_ea_sns_blocked;

    switch(codec) {
        case 0x04: /* "Xas1": EA-XAS (Dead Space) */
            vgmstream->coding_type = coding_EA_XAS;
            break;

        case 0x00: /* "NONE" */
        case 0x01: /* not used? */
        case 0x02: /* "P6B0": PCM16BE */
        case 0x03: /* "EXm0": EA-XMA */
        case 0x05: /* "EL31": EALayer3 v1 b (with PCM blocks in normal EA-frames?) */
        case 0x06: /* "EL32P": EALayer3 v2 "P" */
        case 0x07: /* "EL32S": EALayer3 v2 "S" */
        case 0x09: /* EASpeex? */
        case 0x0c: /* EAOpus? */
        case 0x0e: /* XAS variant? */
        case 0x0f: /* EALayer3 variant? */
        /* also 0x1n variations, used in other headers */
        default:
            VGM_LOG("EA SNU: unknown codec 0x%02x\n", codec);
            goto fail;
    }


    /* open the file for reading by each channel */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    ea_sns_block_update(start_offset, vgmstream);

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
