#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"


/* .ADPCM - from Capcom games [Resident Evil: Revelations (Switch), Monster Hunter XX (Switch)] */
VGMSTREAM* init_vgmstream_adpcm_capcom(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (read_u32be(0x00,sf) != 0x02000000)
        return NULL;
    /* .adpcm: common
     * .mca: Monster Hunter Generations Ultimate / XX */
    if (!check_extensions(sf,"adpcm,mca"))
        return NULL;

    channels = read_u16le(0x04, sf);
    if (channels < 1 || channels > 2)
        return NULL;
    int interleave = read_u16le(0x06, sf);
    if (interleave != 0x100) //even in mono
        return NULL;
    int channel_size = read_u32le(0x08, sf);
    if (channel_size == 0 || channel_size * channels > get_streamfile_size(sf))
        return NULL;

    /* 0x0c-14: some config/id? (may be shared between files) */
    loop_flag = read_s16le(0x68, sf);

    // header seems fixed for mono/stereo
    start_offset = 0xd8;


    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ADPCM_CAPCOM;
    vgmstream->sample_rate = read_s32le(0x64,sf); /* from first header repeated at +0x60 */
    vgmstream->num_samples = read_s32le(0x60, sf);
    vgmstream->loop_start_sample = read_s32le(0x6c, sf);
    vgmstream->loop_end_sample   = read_s32le(0x70, sf) + 1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs_le(vgmstream,sf, 0x18, 0x60);

    if (!vgmstream_open_stream(vgmstream,sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
