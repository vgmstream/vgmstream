#include "meta.h"
#include "../coding/coding.h"

 /* HD3+BD3 - Sony PS3 bank format [Elevator Action Deluxe (PS3), R-Type Dimensions (PS3)] */
VGMSTREAM* init_vgmstream_hd3_bd3(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    uint32_t stream_offset, stream_size;
    int channels, loop_flag, sample_rate, interleave;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "P3HD"))
        return NULL;
    if (!check_extensions(sf, "hd3"))
        return NULL;

    sb = open_streamfile_by_ext(sf,"bd3");
    if (!sf) goto fail;

    /* 0x04: section size (not including first 0x08) */
    /* 0x08: version? 0x00020000 */
    /* 0x10: "P3PG" offset (seems mostly empty and contains number of subsongs towards the end) */
    /* 0x14: "P3TN" offset (some kind of config/volumes/etc?) */
    /* 0x18: "P3VA" offset (VAG headers) */
    {
        uint32_t section_offset = read_u32be(0x18,sf);
        uint32_t section_size;
        int entries;

        if (!is_id32be(section_offset+0x00,sf, "P3VA"))
            goto fail;
        section_size = read_u32be(section_offset+0x04,sf); /* (not including first 0x08) */
        /* 0x08 size of all subsong headers + 0x10 */

        entries = read_u32be(section_offset+0x14,sf);
        /* often there is an extra subsong than written, but may be padding instead */
        if (read_u32be(section_offset + 0x20 + entries*0x10 + 0x04,sf)) /* has sample rate */
            entries += 1;

        if (entries * 0x10 > section_size) /* just in case, padding after entries is possible */
            goto fail;

        /* very often subsongs make stereo pairs (even with many), this detects simple cases */ //TODO check if there is some flag or remove hack
        bool is_bgm = false;
        if (entries == 2) {
            uint32_t curr_size = read_u32be(section_offset+0x20+(0*0x10)+0x08,sf);
            uint32_t next_size = read_u32be(section_offset+0x20+(1*0x10)+0x08,sf);
            is_bgm = (curr_size == next_size);
        }

        if (is_bgm) {
            total_subsongs = 1;
            channels = entries;
        }
        else {
            total_subsongs = entries;
            channels = 1;
        }
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        uint32_t header_offset = section_offset+0x20+(target_subsong-1)*0x10;
        stream_offset = read_u32be(header_offset+0x00,sf);
        sample_rate  = read_u32be(header_offset+0x04,sf);
        stream_size  = read_u32be(header_offset+0x08,sf) * channels;
        interleave = stream_size / channels;
        if (read_s32be(header_offset+0x0c,sf) != -1)
            goto fail;
    }

    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->meta_type = meta_HD3_BD3;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sb, stream_offset))
        goto fail;

    close_streamfile(sb);
    return vgmstream;
fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
