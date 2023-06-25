#include "meta.h"
#include "../coding/coding.h"

/* YMF - from Yuke's games [WWE WrestleMania X8 (GC)] */
VGMSTREAM* init_vgmstream_ymf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag;


    /* checks */
    if (read_u32be(0x00,sf) != 0x00000180 || 
        read_u32be(0x08,sf) != 0x00000003 ||
        read_u32be(0x0c,sf) != 0xCCCCCCCC)
        return NULL;
    /* 0x04: used data size? */

    /* .ymf: actual extension */
    if (!check_extensions(sf, "ymf"))
        return NULL;
    
    /* .ymf can contain audio or video, but not both (videos start with 0x100 and change minor values),
     * though it's are found in ./movie/... and probably are considered so */

    loop_flag = 0;
    channels = 2;
    start_offset = read_u32be(0x00,sf);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_YMF;
    vgmstream->sample_rate = read_32bitBE(0xA8,sf);
    vgmstream->num_samples = read_32bitBE(0xDC,sf);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x20000;

    dsp_read_coefs_be(vgmstream, sf, 0xAE, 0x60);
    //dsp_read_hist_be(vgmstream, sf, 0xAE + 0x20, 0x60);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
