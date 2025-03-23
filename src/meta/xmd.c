#include "meta.h"
#include "../coding/coding.h"

/* XMD - from Konami Xbox games [Silent Hill 4 (Xbox), Castlevania: Curse of Darkness (Xbox)] */
VGMSTREAM* init_vgmstream_xmd(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    size_t data_size, loop_start, frame_size;


    /* checks (.xmd comes from bigfiles with filenames) */
    if (!check_extensions(sf, "xmd"))
        return NULL;

    if ((read_u32be(0x00,sf) & 0xFFFFFF00) == get_id32be("xmd\0")) {
        /* v2 from Castlevania: Curse of Darkness */
        channels    = read_u8   (0x03,sf);
        sample_rate = read_u16le(0x04, sf);
        data_size   = read_u32le(0x06, sf);
        loop_flag   = read_u8   (0x0a,sf);
        loop_start  = read_u32le(0x0b, sf);
        /* 0x0f(2): unknown+config? */

        frame_size = 0x15;
        start_offset = 0x11;
    }
    else {
        // v1 from Silent Hill 4
        channels    = read_u8   (0x00,sf);
        // 01: volume? always 0x80
        sample_rate = read_u16le(0x01, sf);
        data_size   = read_u32le(0x03, sf);
        loop_flag   = read_u8   (0x07,sf);
        loop_start  = read_u32le(0x08, sf);

        frame_size = 0x0d;
        start_offset = 0x0c;
    }

    /* extra checks just in case */
    if (data_size > get_streamfile_size(sf))
        return NULL; // v1 .xmd are sector-aligned with padding
    if (channels < 1 || channels > 2)
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = data_size / frame_size / channels * ((frame_size - 0x06) * 2 + 2); // bytes to samples
    vgmstream->loop_start_sample = loop_start / frame_size / channels * ((frame_size - 0x06)*2 + 2); // bytes to samples
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_XMD;
    vgmstream->coding_type = coding_XMD;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
