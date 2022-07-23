#include "meta.h"
#include "../coding/coding.h"

/* ASF - Argonaut PC games [Croc 2 (PC), Aladdin: Nasira's Revenge (PC)] */
VGMSTREAM* init_vgmstream_asf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int loop_flag, channels, type, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "ASF\0"))
        goto fail;

    /* .asf: original
     * .lasf: fake for plugins */
    if (!check_extensions(sf, "asf,lasf"))
        goto fail;

    if (read_u32le(0x04,sf) != 0x00010002) /* v1.002? */
        goto fail;
    if (read_u32le(0x08,sf) != 0x01 &&
        read_u32le(0x0c,sf) != 0x18)
        goto fail;
    /* 0x10~18: stream name (same as filename) */
    /* 0x18: non-full size? */
    if (read_u32le(0x1c,sf) != 0x20) /* samples per frame? */
        goto fail;
    sample_rate = read_u16le(0x24, sf);

    type = read_u32le(0x28,sf); /* assumed? */
    switch(type){
        case 0x0d: channels = 1; break; /* Aladdin: Nasira's Revenge (PC) */
        case 0x0f: channels = 2; break; /* Croc 2 (PC), The Emperor's New Groove (PC) */
        default: goto fail;
    }

    loop_flag = 0;
    start_offset = 0x2c;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_ASF;
    vgmstream->coding_type = coding_ASF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x11;
    vgmstream->num_samples = (get_streamfile_size(sf) - start_offset) / (0x11 * channels) * 32; /* bytes_to_samples */
    //vgmstream->num_samples = read_32bitLE(0x18,sf) * (32 << channels); /* something like this? */

    read_string(vgmstream->stream_name,0x10, 0x08+1,sf);


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
