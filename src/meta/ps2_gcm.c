#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .GCM - from PS2 Namco games [Gunvari Collection + Time Crisis (PS2), NamCollection (PS2)] */
VGMSTREAM* init_vgmstream_ps2_gcm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, name_offset;
    uint32_t vagp_l_offset, vagp_r_offset, track_size, data_size, channel_size;
    int channels, sample_rate, interleave;


    /* checks */
    if (!is_id32be(0x00,sf, "MCG\0"))
        goto fail;

    /* .gcm: actual extension */
    if (!check_extensions(sf, "gcm"))
        goto fail;


    /* format is two v4 "VAGp" headers then interleaved data (even for 6ch files) */
    vagp_l_offset = read_u32le(0x04, sf);
    if (!is_id32be(vagp_l_offset,sf, "VAGp"))
        goto fail;
    vagp_r_offset = read_u32le(0x08, sf);
    if (!is_id32be(vagp_r_offset,sf, "VAGp"))
        goto fail;

    start_offset = read_u32le(0x0c, sf);
    track_size = read_u32le(0x10, sf); /* stereo size */
    interleave = read_s32le(0x14, sf);
    /* 0x18/1c: null */

    /* nothing in the header indicates multi-channel files (*M.GCM), check expected sizes of stereo pairs */
    data_size = get_streamfile_size(sf) - start_offset;
    if (data_size == (track_size * 3)) {
        channels = 6;
    }
    else if (data_size == track_size) {
        channels = 2;
    }
    else {
        goto fail; /* unknown setup (could also check start frames are null) */
    }

    /* get some values from the VAGp */
    channel_size = read_u32be(vagp_l_offset + 0x0c, sf); /* without padding */
    sample_rate = read_s32be(vagp_l_offset + 0x10, sf);
    name_offset = vagp_l_offset + 0x20; /* both VAGp use the same name (sometimes with an L/R letter) */

    if (channel_size != read_u32be(vagp_r_offset + 0x0c, sf)) /* unlikely... */
        goto fail;
    if (sample_rate != read_u32be(vagp_r_offset + 0x10, sf)) /* unlikely.. */
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_GCM;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    read_string(vgmstream->stream_name,0x10+1, name_offset,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
