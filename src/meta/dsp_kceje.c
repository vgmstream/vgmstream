#include "meta.h"
#include "../coding/coding.h"

/* .dsp - from KCE Japan East GC games [Yu-Gi-Oh! The Falsebound Kingdom (GC), Hikaru No Go 3 (GC)] */
VGMSTREAM* init_vgmstream_dsp_kceje(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    off_t start_offset;


    /* checks */
    if (read_u32be(0x00,sf) + 0xE0 != get_streamfile_size(sf))
        return NULL;
    if (read_u32be(0x04,sf) != 0x01)
        return NULL;
    if (read_u32be(0x08,sf) != 0x10000000)
        return NULL;
    if (read_u32be(0x0c,sf) != 0x00)
        return NULL;

    /* .dsp: assumed (no names in .pac bigfile and refs to DSP streams) */
    if (!check_extensions(sf, "dsp"))
        return NULL;

    channels = 1;
    loop_flag = read_u16be(0x2C,sf) != 0x00;
    start_offset = 0xE0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_u32be(0x28,sf);
    vgmstream->num_samples = read_u32be(0x20,sf);
    vgmstream->loop_start_sample = dsp_bytes_to_samples(read_u32be(0x30,sf), 2);
    vgmstream->loop_end_sample = dsp_bytes_to_samples(read_u32be(0x34,sf), 2);
    vgmstream->allow_dual_stereo = true;

    vgmstream->meta_type = meta_DSP_KCEJE;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    dsp_read_coefs_be(vgmstream, sf, 0x3c, 0x00);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
