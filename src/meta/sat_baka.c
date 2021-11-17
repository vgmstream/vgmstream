#include "meta.h"
#include "../util.h"

/* BAKA - from KCET games [Crypt Killer (Saturn)] */
VGMSTREAM* init_vgmstream_sat_baka(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "BAKA"))
        goto fail;

    /* (extensionless): original
     * .baka: header id */
    if (!check_extensions(sf, ",baka"))
        goto fail;

    /* RIFF style chunks */
    if (!is_id32be(0x08,sf, " AHO") || 
        !is_id32be(0x0C,sf, "PAPA") ||
        !is_id32be(0x26,sf, "MAMA"))
        goto fail;

    //todo begloop markers at EOF
    loop_flag = 0;
    channels = 2;
    start_offset = 0x2E;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = read_u32be(0x16,sf);

    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_SAT_BAKA;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
