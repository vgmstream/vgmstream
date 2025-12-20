#include "meta.h"
#include "../coding/coding.h"

/* MSB+MSH - SCEE MultiStream flat bank [namCollection: Ace Combat 2 (PS2) sfx, EyeToy Play (PS2)] */
VGMSTREAM* init_vgmstream_msh_msb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    off_t start_offset, header_offset = 0;
    size_t stream_size;
    int loop_flag, channels, sample_rate;
    int32_t  loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;
    uint32_t config;

    /* checks */
    if (read_u32le(0x00,sf) != get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "msh"))
        return NULL;
    /* 0x04: flags? (0x04/34*/

    /* parse entries */
    {
        int entries = read_s32le(0x08,sf); /* may be less than file size, or include dummies (all dummies is possible too) */

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        for (int i = 0; i < entries; i++) {
            if (read_u32le(0x0c + 0x10*i, sf) == 0) /* size 0 = empty entry */
                continue;

            total_subsongs++;
            if (total_subsongs == target_subsong && !header_offset) {
                header_offset = 0x0c + 0x10*i;
            }
        }

        if (!header_offset) return NULL;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) return NULL;
    }


    stream_size  = read_u32le(header_offset+0x00, sf);
    config       = read_u32le(header_offset+0x04, sf); /* volume (0~100), null, null, loop (0/1) */
    start_offset = read_u32le(header_offset+0x08, sf);
    sample_rate  = read_u32le(header_offset+0x0c, sf); /* Ace Combat 2 seems to set wrong values but probably their bug */

    loop_flag = (config & 1);
    channels = 1;

    /* rare [Dr. Seuss Cat in the Hat (PS2)] */
    if (loop_flag) {
        /* when loop is set ADPCM has loop flags, but rarely appear too without loop set */
        loop_flag = ps_find_loop_offsets(sf, start_offset, stream_size, channels, 0, &loop_start, &loop_end);
    }

    sb = open_streamfile_by_ext(sf, "msb");
    if (!sb) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSH_MSB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
