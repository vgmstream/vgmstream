#include "meta.h"
#include "../coding/coding.h"

static VGMSTREAM* init_vgmstream_multistream(STREAMFILE* sf_head, STREAMFILE* sf_body, off_t header_offset, off_t start_offset);

/* MIH+MIB - SCEE MultiStream interleaved bank (header+data) [namCollection: Ace Combat 2 (PS2), Rampage: Total Destruction (PS2)] */
VGMSTREAM* init_vgmstream_mib_mih(STREAMFILE* sf_body) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_head = NULL;
    off_t header_offset, start_offset;

    /* check extension */
    if (!check_extensions(sf_body, "mib"))
        return NULL;

    sf_head = open_streamfile_by_ext(sf_body, "mih");
    if (!sf_head) goto fail;

    header_offset = 0x00;
    start_offset = 0x00;

    if (read_u32le(0x00, sf_head) != 0x40) { /* header size */
        /* Marc Ecko's Getting Up (PS2) has a name at the start (hack, not standard .mib+mih) */
        size_t name_size = read_u32le(0x00, sf_head);
        if (read_u32le(0x04 + name_size + 0x00, sf_head) == 0x40 &&
            read_u32le(0x04 + name_size + 0x04, sf_head) == 0x40) {
            header_offset = 0x04 + name_size + 0x04;
        } else {
            goto fail;
        }
    }

    vgmstream = init_vgmstream_multistream(sf_head, sf_body, header_offset, start_offset);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MIB_MIH;

    close_streamfile(sf_head);
    return vgmstream;

fail:
    close_streamfile(sf_head);
    close_vgmstream(vgmstream);
    return NULL;
}

/* MIC - SCEE MultiStream interleaved bank (merged MIH+MIB) [Rogue Trooper (PS2), The Sims 2 (PS2)] */
VGMSTREAM* init_vgmstream_mic(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t header_offset, start_offset;

    /* check extension */
    /* .mic: official extension
     * (extensionless): The Urbz (PS2), The Sims 2 series (PS2) */
    if (!check_extensions(sf, "mic,"))
        return NULL;
    if (read_u32le(0x00, sf) != 0x40) /* header size */
        return NULL;

    header_offset = 0x00;
    start_offset = 0x40;

    vgmstream = init_vgmstream_multistream(sf, sf, header_offset, start_offset);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MIC;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* init_vgmstream_multistream(STREAMFILE* sf_head, STREAMFILE* sf_body, off_t header_offset, off_t start_offset) {
    VGMSTREAM* vgmstream = NULL;
    size_t data_size, frame_size, frame_last, frame_count;
    int channels, loop_flag, sample_rate;

    loop_flag = 0; /* MIB+MIH/MIC don't loop (nor use PS-ADPCM flags) per spec */

    /* 0x04: padding size (always 0x20, MIH header must be multiple of 0x40) */
    //if (read_u8(header_offset + 0x04, sf_head) != 0x20) goto fail;
    frame_last  = read_u32le(header_offset + 0x04, sf_head) >> 8; /* 24b */
    channels    = read_u32le(header_offset + 0x08, sf_head);
    sample_rate = read_u32le(header_offset + 0x0c, sf_head);
    frame_size  = read_u32le(header_offset + 0x10, sf_head);
    frame_count = read_u32le(header_offset + 0x14, sf_head);
    if (frame_count == 0) { /* rarely [Gladius (PS2)] */
        frame_count = (get_streamfile_size(sf_body) - start_offset) / (frame_size * channels);
    }

    data_size = frame_count * frame_size;
    if (frame_last)
        data_size -= frame_size - frame_last; /* last frame has less usable data */
    data_size *= channels;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream, sf_body, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
