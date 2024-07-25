#include "meta.h"
#include "../coding/coding.h"

/* RSTM - from Rockstar games [Midnight Club 3, Bully - Canis Canim Edit (PS2)] */
VGMSTREAM* init_vgmstream_rstm_rockstar(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t stream_offset, loop_start, loop_end;
    size_t stream_size;
    int sample_rate, channels, loop_flag;


    /* checks */
    if (!is_id32be(0x00, sf, "RSTM"))
        return NULL;

    /* .rsm: in filelist
     * .rstm: header id */
    if (!check_extensions(sf, "rsm,rstm"))
        return NULL;

    sample_rate = read_s32le(0x08, sf);
    channels    = read_s32le(0x0C, sf);
    /* 0x10-0x18 - empty padding(?) */
    stream_size = read_s32le(0x18, sf);
    loop_start  = read_s32le(0x1C, sf);
    loop_end    = read_s32le(0x20, sf);
    /* other loop start/ends after here? (uncommon) */
    stream_offset = 0x800;

    //loop_flag = (read_s32le(0x24,sf) > 0);
    loop_flag = loop_end != stream_size;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RSTM_ROCKSTAR;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, sf, stream_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
