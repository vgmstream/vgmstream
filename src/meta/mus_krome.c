#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* .mus - from Krome games [Ty: The Tasmanian Tiger 2 (GC), Star Wars: The Force Unleashed (Wii)] */
VGMSTREAM* init_vgmstream_mus_krome(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int channels, loop_flag, interleave;
    int32_t num_samples;


    /* checks */
    num_samples = read_s32be(0x00,sf);
    interleave = read_s32be(0x04,sf);
    start_offset = read_u32be(0x08,sf);
    data_size = read_u32be(0x0c,sf);

    if (interleave != 0x8000)
        return NULL;
    if (start_offset != 0x80)
        return NULL;
    if (data_size + start_offset != get_streamfile_size(sf))
        return NULL;
    /* could test gain/initial ps at 0x10 + 0x20 too */

    if (!check_extensions(sf,"mus"))
        return NULL;


    channels = 2;
    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MUS_KROME;
    vgmstream->num_samples = num_samples;
    vgmstream->sample_rate = read_u16be(0x6c,sf);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave; /* no last block size unlike similar DSPs */

    dsp_read_coefs_be(vgmstream, sf, 0x10, 0x2e);
    dsp_read_hist_be(vgmstream, sf, 0x10 + 0x24, 0x2e);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
