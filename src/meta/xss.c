#include "meta.h"
#include "../coding/coding.h"


/* .XSS - found in Dino Crisis 3 (Xbox) */
VGMSTREAM* init_vgmstream_xss(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag, sample_rate;

    /* checks */
    /* 00: name + garbage data up to ? 
     * (due to garbage it's hard which values are from this header, but these seem consistent) */
    if (read_u32le(0x0c,sf) != 0x3a)
        return NULL;
    if (read_u32le(0x10,sf) != 0x00)
        return NULL;

    if (!check_extensions(sf,"xss"))
        return NULL;

    /* some floats and stuff up to 0x100, then some sizes, then RIFF fmt-like header */
    
    uint32_t head_offset = 0x140; 
    if (read_u32le(head_offset+0x00, sf) != 0x40)
        return NULL;

    loop_flag = (read_u32le(head_offset + 0x04,sf) != 0);
    if (read_u16le(head_offset + 0x0c,sf) != 0x01)
        return NULL;
    channels = read_u16le(head_offset + 0x0E,sf);
    sample_rate = read_u32le(head_offset + 0x10,sf);
    /* 14: bitrate */
    /* 18: block size */
    if (read_u16le(head_offset + 0x1A,sf) != 0x10)
        return NULL;

    start_offset = 0x800;
    

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XSS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm16_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels);
    vgmstream->loop_start_sample = pcm16_bytes_to_samples(read_u32le(head_offset + 0x04,sf), channels);
    vgmstream->loop_end_sample = pcm16_bytes_to_samples(read_u32le(head_offset + 0x08,sf), channels);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
