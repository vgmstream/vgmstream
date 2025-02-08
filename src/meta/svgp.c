#include "meta.h"
#include "../coding/coding.h"

/* SVGp - from High Voltage games [Hunter: The Reckoning - Wayward (PS2)] */
VGMSTREAM* init_vgmstream_svgp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size, interleave;
    int loop_flag, channels;
    int32_t loop_start = 0, loop_end = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "SVGp"))
        return NULL;
    if (!check_extensions(sf,"svg"))
        return NULL;

    start_offset = 0x800;
    data_size = read_u32le(0x18,sf);
    interleave = read_u32le(0x14,sf);
    channels = 2;
    loop_flag = ps_find_loop_offsets(sf, start_offset, data_size, channels, interleave,&loop_start, &loop_end);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SVGP;
    vgmstream->sample_rate = read_s32be(0x2c,sf);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channels);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    read_string(vgmstream->stream_name,0x10+1, 0x04,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
