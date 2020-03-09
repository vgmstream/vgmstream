#include "meta.h"
#include "../coding/coding.h"

/* NPFS - found in Namco NuSound v1 games [Tekken 5 (PS2), Venus & Braves (PS2), Ridge Racer (PSP)] */
VGMSTREAM* init_vgmstream_nps(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    uint32_t channel_size;
    int loop_flag, channel_count, loop_start, sample_rate;


    /* checks */
    /* .nps: referenced extension (ex. Venus & Braves, Ridge Racer data files)
     * .npsf: header id (Namco Production Sound File?) */
    if ( !check_extensions(sf,"nps,npsf"))
        goto fail;

    if (read_u32be(0x00, sf) != 0x4E505346) /* "NPSF" */
        goto fail;

    /* 0x04: version? (0x00001000 = 1.00?) */
    channel_size    = read_s32le(0x08, sf);
    channel_count   = read_s32le(0x0C, sf);
    start_offset    = read_s32le(0x10, sf); /* interleave? */
    loop_start      = read_s32le(0x14, sf);
    sample_rate     = read_s32le(0x18, sf);
    /* 0x1c: volume? (0x3e8 = 1000 = max) */
    /* 0x20: flags? (varies between sound types in a game, but no clear pattern vs other games) */
    /* 0x24: flag? (0/1) */
    /* 0x28: null */
    /* 0x2c: null */
    /* 0x30: always 0x40 */
    /* 0x34: name (usually null terminated but may contain garbage) */
    /* rest: null or 0xFF until start */
    loop_flag = loop_start != -1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;
    vgmstream->meta_type = meta_NPS;
    read_string(vgmstream->stream_name, STREAM_NAME_SIZE, 0x34, sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
