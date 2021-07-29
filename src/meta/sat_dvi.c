#include "meta.h"

/* DVI - from Konami KCE Nayoga SAT games (Castlevania Symphony of the Night, Jikkyou Oshaberi Parodius - Forever with Me) */
VGMSTREAM* init_vgmstream_sat_dvi(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks
     * .pcm: original
     * .dvi: header id (to be removed )*/
    if (!check_extensions(sf,"pcm,dvi"))
        goto fail;

    if (!is_id32be(0x00,sf, "DVI."))
        goto fail;

    start_offset = read_s32be(0x04,sf);
    loop_flag = (read_s32be(0x0C,sf) != -1);
    channels = 2; /* no mono files seem to exists */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = read_s32be(0x08,sf);
    vgmstream->loop_start_sample = read_s32be(0x0C,sf);
    vgmstream->loop_end_sample = read_s32be(0x08,sf);

    vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x4;
    vgmstream->meta_type = meta_SAT_DVI;

    /* at 0x10 (L) / 0x20 (R): probably ADPCM loop history @+0x00 and step @+0x17 (not init values) */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;

    /* for some reason right channel goes first (tested in SOTN vs emu and PS/OST version), swap offsets */
    if (channels == 2) {
        off_t temp = vgmstream->ch[0].offset;
        vgmstream->ch[0].channel_start_offset =
                vgmstream->ch[0].offset = vgmstream->ch[1].offset;
        vgmstream->ch[1].channel_start_offset =
                vgmstream->ch[1].offset = temp;
    }

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
