#include "meta.h"
#include "../coding/coding.h"

/* SMH+SMC - from Wangan Midnight 1/R (System246) */
VGMSTREAM* init_vgmstream_smh_smc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    off_t start_offset, header_offset = 0;
    size_t stream_size;
    int loop_flag = 0, channels, sample_rate;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    total_subsongs = read_s32le(0x00,sf);
    if (total_subsongs * 0x10 + 0x10 != get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "smh"))
        return NULL;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = 0x10 + (target_subsong - 1) * 0x10;

    start_offset  = read_u32le(header_offset+0x00, sf);
    stream_size   = read_u32le(header_offset+0x04, sf);
    sample_rate   = read_u32le(header_offset+0x08, sf);
    // 0x0c(2): always 0x10, frame size?
    channels      = read_u16le(header_offset+0x0e, sf);
    loop_flag = 0;

    sb = open_streamfile_by_ext(sf, "smc");
    if (!sb) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SMH_SMC;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x04, sf);


    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;

    close_streamfile(sb);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
