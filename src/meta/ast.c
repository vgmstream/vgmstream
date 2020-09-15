#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* .AST - from Nintendo games [Super Mario Galaxy (Wii), Pac-Man Vs (GC)] */
VGMSTREAM * init_vgmstream_ast(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, codec;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(streamFile, "ast"))
        goto fail;

    if (((uint32_t)read_32bitBE(0x00, streamFile) == 0x5354524D) && /* "STRM" */
        ((uint32_t)read_32bitBE(0x40, streamFile) == 0x424C434B)) { /* "BLCK" */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    } else if (((uint32_t)read_32bitBE(0x00, streamFile) == 0x4D525453) && /* "MRTS" */ // Super Mario Galaxy (Super Mario 3D All-Stars (Switch))
               ((uint32_t)read_32bitBE(0x40, streamFile) == 0x4B434C42)) { /* "KCLB" */
               read_32bit = read_32bitLE;
               read_16bit = read_16bitLE;
               big_endian = 0;
    } else {
        goto fail;
    }

    if (read_16bit(0x0a,streamFile) != 0x10) /* ? */
        goto fail;

    if (read_32bit(0x04,streamFile)+0x40 != get_streamfile_size(streamFile))
        goto fail;

    codec         = read_16bitBE(0x08,streamFile); // always big-endian?
    channel_count = read_16bit(0x0c,streamFile);
    loop_flag     = read_16bit(0x0e,streamFile);
    //max_block   = read_32bit(0x20,streamFile);
    start_offset  = 0x40;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AST;
    vgmstream->sample_rate = read_32bit(0x10,streamFile);
    vgmstream->num_samples = read_32bit(0x14,streamFile);
    vgmstream->loop_start_sample = read_32bit(0x18,streamFile);
    vgmstream->loop_end_sample = read_32bit(0x1c,streamFile);
    vgmstream->codec_endian = big_endian;

    vgmstream->layout_type = layout_blocked_ast;
    switch (codec) {
        case 0x00: /* , Pikmin 2 (GC) */
            vgmstream->coding_type = coding_NGC_AFC;
            break;
        case 0x01: /* Mario Kart: Double Dash!! (GC) */
            vgmstream->coding_type = coding_PCM16BE;
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
