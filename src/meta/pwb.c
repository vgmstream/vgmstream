#include "meta.h"
#include "../coding/coding.h"

/* WB - from Psychonauts (PS2) */
VGMSTREAM* init_vgmstream_pwb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    uint32_t stream_offset, stream_size, loop_start, loop_end;


    /* checks */
    if (!is_id32be(0x00, sf, "WB\2\0"))
        return NULL;

    /* .pwb: actual extension (bigfile has only hashes but there are names in internal files)
     *       (some .pwb have a companion .psb, seems cue-related) */ 
    if (!check_extensions(sf, "pwb"))
        return NULL;
    
    /* 00: ID
     * 04: null
     * 08: header offset? (0x20)
     * 0c: header size? (0x20)
     * 10: entries offset
     * 14: entries size
     * 18: data offset
     * 1c: data size
     * 20: always 1 (channels? codec?)
     * 24: entries count
     * 28: entry size
     * 2c: data offset
     */ 


    stream_offset = read_u32le(0x18, sf);

    int total_subsongs = read_s32le(0x24, sf);
    int target_subsong = sf->stream_index;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;


    {
        uint32_t offset = read_u32le(0x10, sf) + (target_subsong - 1) * read_u32le(0x28, sf);

        /* 0x00: flags? */
        /* 0x04: always 000AC449 */
        stream_offset = read_u32le(offset + 0x08, sf) + stream_offset;
        stream_size  = read_u32le(offset + 0x0c, sf);
        loop_start  = read_u32le(offset + 0x10, sf);
        loop_end = read_u32le(offset + 0x14, sf) + loop_start;
        loop_flag = loop_end; /* both 0 if no loop */
        channels = 1;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PWB;
    vgmstream->sample_rate = 24000;

    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf);
    return NULL;
}
