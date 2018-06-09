#include "meta.h"
#include "../coding/coding.h"

/* MIC/MIHB - SCEE MultiStream interleaved bank (merged MIH+MIB) [Rogue Trooper (PS2), The Sims 2 (PS2)] */
VGMSTREAM * init_vgmstream_ps2_mihb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size, frame_size, frame_last, frame_count;
    int channel_count, loop_flag;

    /* check extension */
    /* .mic: official extension, .mihb: assumed? */
    if (!check_extensions(streamFile, "mic,mihb"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x40000000) /* header size */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x08,streamFile);
    start_offset = 0x40;

    /* frame_size * frame_count * channels = data_size, but last frame has less usable data */
    {
        /* 0x04: padding (0x20, MIH header must be multiple of 0x40) */
        frame_last  = (uint16_t)read_16bitLE(0x05,streamFile);
        frame_size  = read_32bitLE(0x10,streamFile);
        frame_count = read_32bitLE(0x14,streamFile);

        data_size  = frame_count * frame_size;
        data_size -= frame_last ? (frame_size-frame_last) : 0;
        data_size *= channel_count;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);

    vgmstream->meta_type = meta_PS2_MIHB;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
