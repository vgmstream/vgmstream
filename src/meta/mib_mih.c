#include "meta.h"
#include "../coding/coding.h"

/* MIB+MIH - SCEE MultiStream interleaved bank (header+data) [namCollection: Ace Combat 2 (PS2), Rampage: Total Destruction (PS2)] */
VGMSTREAM* init_vgmstream_mib_mih(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sh = NULL;
    off_t header_offset, start_offset;
    size_t data_size, frame_size, frame_last, frame_count;
    int channels, loop_flag, sample_rate;

    /* check extension */
    if (!check_extensions(sf, "mib"))
        goto fail;

    sh = open_streamfile_by_ext(sf,"mih");
    if (!sh) goto fail;

    header_offset = 0x00;

    if (read_u32le(0x00,sh) != 0x40) { /* header size */
        /* Marc Ecko's Getting Up (PS2) has a name at the start (hack, not standard .mib+mih) */
        size_t name_size = read_u32le(0x00, sh);
        if (read_u32le(0x04 + name_size, sh) == 0x40 &&
            read_u32le(0x04 + name_size + 0x04, sh) == 0x40) {
            header_offset = 0x04 + name_size + 0x04;
        } else {
            goto fail;
        }
    }

    loop_flag = 0; /* MIB+MIH don't loop (nor use PS-ADPCM flags) per spec */
    start_offset = 0x00;

    /* 0x04: padding size (always 0x20, MIH header must be multiple of 0x40) */
    frame_last      = read_u32le(header_offset + 0x05,sh) & 0x00FFFFFF; /* 24b */
    channels        = read_u32le(header_offset + 0x08,sh);
    sample_rate     = read_u32le(header_offset + 0x0c,sh);
    frame_size      = read_u32le(header_offset + 0x10,sh);
    frame_count     = read_u32le(header_offset + 0x14,sh);
    if (frame_count == 0) { /* rarely [Gladius (PS2)] */
        frame_count = get_streamfile_size(sf) / (frame_size * channels);
    }

    data_size  = frame_count * frame_size;
    if (frame_last)
        data_size -= frame_size - frame_last; /* last frame has less usable data */
    data_size *= channels;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MIB_MIH;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    close_streamfile(sh);
    return vgmstream;

fail:
    close_streamfile(sh);
    close_vgmstream(vgmstream);
    return NULL;
}
