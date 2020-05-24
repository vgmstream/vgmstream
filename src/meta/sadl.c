#include "meta.h"

/* sadl - from DS games with Procyon Studio audio driver [Professor Layton (DS), Soma Bringer (DS)] */
VGMSTREAM* init_vgmstream_sadl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channel_count, loop_flag;
    off_t start_offset;


    /* checks */
    if (!check_extensions(sf, "sad"))
        goto fail;

    if (read_32bitBE(0x00,sf) != 0x7361646c) /* "sadl" */
        goto fail;
    if (read_32bitLE(0x40,sf) != get_streamfile_size(sf))
        goto fail;

    loop_flag = read_8bit(0x31,sf);
    channel_count = read_8bit(0x32,sf);
    start_offset = 0x100;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    switch (read_8bit(0x33,sf) & 6) {
        case 4:
            vgmstream->sample_rate = 32728;
            break;
        case 2:
            vgmstream->sample_rate = 16364;
            break;
        default:
            goto fail;
    }

    vgmstream->meta_type = meta_SADL;

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    switch(read_8bit(0x33,sf) & 0xf0) {
        case 0x70: /* Ni no Kuni (DS), Professor Layton and the Curious Village (DS), Soma Bringer (DS) */
            vgmstream->coding_type = coding_IMA_int;

            vgmstream->num_samples = (read_32bitLE(0x40,sf)-start_offset)/channel_count*2;
            vgmstream->loop_start_sample = (read_32bitLE(0x54,sf)-start_offset)/channel_count*2;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        case 0xb0: /* Soma Bringer (DS), Rekishi Taisen Gettenka (DS) */
            vgmstream->coding_type = coding_NDS_PROCYON;

            vgmstream->num_samples = (read_32bitLE(0x40,sf)-start_offset)/channel_count/16*30;
            vgmstream->loop_start_sample = (read_32bitLE(0x54,sf)-start_offset)/channel_count/16*30;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
