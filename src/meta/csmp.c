#include "meta.h"
#include "../coding/coding.h"


/* CSMP - Retro Studios sample [Metroid Prime 3 (Wii), Donkey Kong Country Returns (Wii)] */
VGMSTREAM* init_vgmstream_csmp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, first_offset = 0x08, chunk_offset;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "csmp"))
        goto fail;
    if (!is_id32be(0x00, sf, "CSMP"))
        goto fail;
    if (read_u32be(0x04, sf) != 1)
        goto fail;

    if (!find_chunk(sf, 0x44415441,first_offset,0, &chunk_offset,NULL, 1, 0)) /*"DATA"*/
        goto fail;

    /* contains standard DSP header, but somehow some validations (start/loop ps)
     * don't seem to work, so no point to handle as standard DSP */

    channels = 1;
    loop_flag = read_s16be(chunk_offset+0x0c,sf);
    start_offset = chunk_offset + 0x60;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_CSMP;
    vgmstream->sample_rate = read_s32be(chunk_offset+0x08,sf);
    vgmstream->num_samples = read_s32be(chunk_offset+0x00,sf);
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_u32be(chunk_offset+0x10,sf));
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(read_u32be(chunk_offset+0x14,sf)) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    dsp_read_coefs_be(vgmstream, sf, chunk_offset+0x1c, 0x00);
    dsp_read_hist_be(vgmstream, sf, chunk_offset+0x40, 0x00);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
