#include "meta.h"
#include "../coding/coding.h"


/* !B0X/CB03 - Traveller's Tales (!B0X) / Warthog (CB03) speech files [Lego Batman 2 (PC), Lego Dimensions (PS3), Animaniacs: The Great Edgar Hunt (GC)] */
VGMSTREAM* init_vgmstream_chatterbox(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, pcm_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "!B0X") && !is_id32be(0x00,sf, "CB03"))
        return NULL;
    // .cbx: Traveller's Tales
    // .box: Warthog
    if (!check_extensions(sf, "cbx,box"))
        return NULL;

    /* debug strings identify this as "Chatterbox"/"CBOX"/"CBX", while sound lib seems called "NuSound"
     * (probably based on .utk) */

    pcm_size = read_u32le(0x04, sf);
    sample_rate = read_s32le(0x08, sf);
    start_offset = 0x0c;
    channels = 1;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_CBX;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_size / 2;
    vgmstream->coding_type = coding_EA_MT;
    vgmstream->layout_type = layout_none;
    vgmstream->codec_data = init_ea_mt_cbx(vgmstream->channels);
    if (!vgmstream->codec_data) goto fail;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
