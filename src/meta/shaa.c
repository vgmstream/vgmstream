#include "meta.h"
#include "../coding/coding.h"

/* SHAA/SHSA: Audio format for Nintendo Sound Clock: Alarmo */
VGMSTREAM* init_vgmstream_shaa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, info_offset, adpcm_offset;
    int channels, loop_flag, loop_start, loop_end, codec;

    /* Validate header */
    if (!is_id32be(0x00, sf, "SHAA"))
        goto fail;

    /* Validate extension */
    if (!check_extensions(sf, "shaa,shsa"))
        goto fail;

    info_offset = 0x10;

    codec = read_u8(info_offset + 0x00, sf);
    channels = 1;   // Not sure if file is stereo or mono, so assuming mono for now

    start_offset = read_u32le(0x08, sf);

    /* Loop start and end points */
    loop_start = read_s32le(info_offset + 0x14, sf);
    loop_end = read_s32le(info_offset + 0x18, sf);
    loop_flag = loop_start + loop_end;  // Loop flag is 0 if loop start and loop end are both 0

    /* Alloc vgmstream */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SHAA;

    vgmstream->sample_rate = read_u16le(info_offset + 0x04, sf);
    vgmstream->num_samples = read_s32le(info_offset + 0x08, sf);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;


    vgmstream->layout_type = layout_none;
    switch (codec) {
    case 1: // PCM16LE: Seen in "factory" sound files
        vgmstream->coding_type = coding_PCM16LE;
        break;
    case 2: // NGC DSP: Used for everything else
        vgmstream->coding_type = coding_NGC_DSP;
        break;
    default:
        goto fail;
    }

    if (vgmstream->coding_type == coding_NGC_DSP) {
        adpcm_offset = read_u32le(info_offset + 0x0C, sf);
        dsp_read_coefs_le(vgmstream, sf, adpcm_offset + 0x1C, 0);   // 0 spacing because there's only 1 channel
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
