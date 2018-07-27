#include "meta.h"
#include "../coding/coding.h"

/* XMD - from Konami Xbox games [Silent Hill 4 (Xbox), Castlevania Curse of Darkness (Xbox)] */
VGMSTREAM * init_vgmstream_xmd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    size_t data_size, loop_start, frame_size;


    /* checks (.xmd comes from bigfiles with filenames) */
    if (!check_extensions(streamFile, "xmd"))
        goto fail;

    if ((read_32bitBE(0x00,streamFile) & 0xFFFFFF00) == 0x786D6400) { /* "xmd\0" */
        /* v2 from Castlevania: Curse of Darkness */
        channel_count = read_8bit(0x03,streamFile);
        sample_rate = (uint16_t)read_16bitLE(0x04, streamFile);
        data_size = read_32bitLE(0x06, streamFile);
        loop_flag = read_8bit(0x0a,streamFile);
        loop_start = read_32bitLE(0x0b, streamFile);
        /* 0x0f(2): unknown+config? */
        frame_size = 0x15;
        start_offset = 0x11;
    }
    else {
        /* v1 from Silent Hill 4 */
        channel_count = read_8bit(0x00,streamFile);
        sample_rate = (uint16_t)read_16bitLE(0x01, streamFile);
        data_size = read_32bitLE(0x03, streamFile);
        loop_flag = read_8bit(0x07,streamFile);
        loop_start = read_32bitLE(0x08, streamFile);

        frame_size = 0x0d;
        start_offset = 0x0c;
    }

    /* extra checks just in case */
    if (data_size > get_streamfile_size(streamFile))
        goto fail; /* v1 .xmd are sector-aligned with padding */
    if (channel_count > 2)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = data_size / frame_size / channel_count * ((frame_size-0x06)*2 + 2); /* bytes to samples */
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start / frame_size / channel_count * ((frame_size-0x06)*2 + 2); /* bytes to samples */
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_XMD;
    vgmstream->coding_type = coding_XMD;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
