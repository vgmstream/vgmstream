#include "meta.h"

/* DVI - from Konami KCE Nayoga SAT games (Castlevania Symphony of the Night, Jikkyou Oshaberi Parodius - Forever with Me) */
VGMSTREAM * init_vgmstream_sat_dvi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension (.pcm: original, .dvi: renamed to header id) */
    if ( !check_extensions(streamFile,"pcm,dvi") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4456492E) /* "DVI." */
        goto fail;

    start_offset = read_32bitBE(0x04,streamFile);
    loop_flag = (read_32bitBE(0x0C,streamFile) != 0xFFFFFFFF);
    channel_count = 2; /* no mono files seem to exists */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = read_32bitBE(0x08,streamFile);
    vgmstream->loop_start_sample = read_32bitBE(0x0C,streamFile);
    vgmstream->loop_end_sample = read_32bitBE(0x08,streamFile);

    vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x4;
    vgmstream->meta_type = meta_SAT_DVI;

    /* at 0x10 (L) / 0x20 (R): probably ADPCM loop history @+0x00 and step @+0x17 (not init values) */

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    /* for some reason right channel goes first (tested in SOTN vs emu and PS/OST version), swap offsets */
    if (channel_count == 2) {
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
