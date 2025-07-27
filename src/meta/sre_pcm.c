#include "meta.h"
#include "../coding/coding.h"

/* .SRE+PCM. - Capcom's header+data container thing [Viewtiful Joe (PS2)] */
VGMSTREAM* init_vgmstream_sre_pcm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    off_t start_offset;
    int loop_flag, channels;
    int table1_entries, table2_entries;
    uint32_t table1_offset, table2_offset;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    table1_entries = read_s32le(0x00, sf);
    if (table1_entries <= 0 || table1_entries >= 0x100) //arbitrary max
        return NULL;
    table1_offset  = read_u32le(0x04, sf);
    table2_entries = read_s32le(0x08, sf);
    table2_offset  = read_u32le(0x0c, sf);
    if (table1_entries * 0x60 + table1_offset != table2_offset)
        return NULL;

    if (!check_extensions(sf, "sre"))
        return NULL;

    total_subsongs = table2_entries;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        return NULL;

    uint32_t header_offset = table2_offset + (target_subsong - 1) * 0x20;

    channels       = read_s32le(header_offset+0x00,sf);
    loop_flag      = read_s32le(header_offset+0x18,sf);
    start_offset   = read_u32le(header_offset+0x08,sf);

    sb = open_streamfile_by_ext(sf, "pcm");
    if (!sb) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_u16le(header_offset+0x04,sf);
    vgmstream->meta_type = meta_SRE_PCM;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x1000;

    vgmstream->num_samples       = ps_bytes_to_samples(read_u32le(header_offset+0x0c,sf), channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_u32le(header_offset+0x10,sf), 1);
    vgmstream->loop_end_sample   = ps_bytes_to_samples(read_u32le(header_offset+0x14,sf), 1);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = read_u32le(header_offset+0x0c,sf);

    if (!vgmstream_open_stream(vgmstream,sb,start_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;
fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
