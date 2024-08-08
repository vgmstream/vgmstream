#include "meta.h"
#include "../coding/coding.h"

/* .PCM - from KCE Japan East PS2 games [Ephemeral Fantasia (PS2), Yu-Gi-Oh! The Duelists of the Roses (PS2), 7 Blades (PS2)] */
VGMSTREAM* init_vgmstream_pcm_kceje(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    uint32_t data_size = read_u32le(0x00,sf);
    if (data_size > 0x00 && data_size + 0x800 >= get_streamfile_size(sf) && data_size + 0x1000 <= get_streamfile_size(sf))
        return NULL; /* usually 0x800 but may be padded */
    if (pcm16_bytes_to_samples(data_size, 2) != read_u32le(0x04,sf))
        return NULL;

    if (!check_extensions(sf,"pcm"))
        return NULL;

    loop_flag = (read_s32le(0x0C,sf) != 0x00);
    channels = 2;
    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = channels;
    vgmstream->sample_rate = 24000;
    vgmstream->num_samples = read_s32le(0x04,sf);
    vgmstream->loop_start_sample = read_s32le(0x08,sf);
    vgmstream->loop_end_sample = read_s32le(0x0C,sf);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;
    vgmstream->meta_type = meta_PCM_KCEJE;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
