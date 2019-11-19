#include "meta.h"
#include "../coding/coding.h"

/* MIB+MIH - SCEE MultiStream interleaved bank (header+data) [namCollection: Ace Combat 2 (PS2), Rampage: Total Destruction (PS2)] */
VGMSTREAM * init_vgmstream_mib_mih(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t header_offset, start_offset;
    size_t data_size, frame_size, frame_last, frame_count, name_size;
    int channel_count, loop_flag, sample_rate;

    /* check extension */
    if (!check_extensions(streamFile, "mib"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile,"mih");
    if (!streamHeader) goto fail;

    header_offset = 0x00;

    if (read_32bitLE(0x00,streamHeader) != 0x40) { /* header size */
        name_size = read_32bitLE(0x00, streamHeader);
        if (read_32bitLE(0x04 + name_size, streamHeader) == 0x40 &&
            read_32bitLE(0x04 + name_size + 0x04, streamHeader) == 0x40) {
            /* Marc Ecko's Getting Up (PS2) has a name at the start */
            header_offset = 0x04 + name_size + 0x04;
        } else {
            goto fail;
        }
    }

    loop_flag = 0; /* MIB+MIH don't loop (nor use PS-ADPCM flags) per spec */
    start_offset = 0x00;

    /* 0x04: padding size (always 0x20, MIH header must be multiple of 0x40) */
    frame_last      = (uint32_t)read_32bitLE(header_offset + 0x05,streamHeader) & 0x00FFFFFF; /* 24b */
    channel_count   = read_32bitLE(header_offset + 0x08,streamHeader);
    sample_rate     = read_32bitLE(header_offset + 0x0c,streamHeader);
    frame_size      = read_32bitLE(header_offset + 0x10,streamHeader);
    frame_count     = read_32bitLE(header_offset + 0x14,streamHeader);
    if (frame_count == 0) { /* rarely [Gladius (PS2)] */
        frame_count = get_streamfile_size(streamFile) / (frame_size * channel_count);
    }

    data_size  = frame_count * frame_size;
    if (frame_last)
        data_size -= frame_size - frame_last; /* last frame has less usable data */
    data_size *= channel_count;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);

    vgmstream->meta_type = meta_PS2_MIB_MIH;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    close_streamfile(streamHeader);
    return vgmstream;

fail:
close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
