#include "meta.h"
#include "../coding/coding.h"


/* MSV - from Sony MultiStream format [Fight Club (PS2), PoPcap Hits Vol. 1 (PS2)] */
VGMSTREAM* init_vgmstream_msv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t channel_size;
    int loop_flag, channels;


    if (!is_id32be(0x00,sf, "MSVp"))
        goto fail;

    /* checks */
    /* .msv: actual extension
     * .msvp: header ID */
    if (!check_extensions(sf,"msv,msvp"))
        goto fail;

    channels = 1;
    channel_size = read_u32be(0x0c,sf);
    loop_flag = 0; /* no looping and last 16 bytes (end frame) are removed, from Sony's docs */
    start_offset = 0x30;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSV;
    vgmstream->sample_rate = read_u32be(0x10,sf);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    read_string(vgmstream->stream_name,0x10+1, 0x20,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
