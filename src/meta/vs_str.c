#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* .vs/STRx - from The Bouncer (PS2) */
VGMSTREAM* init_vgmstream_vs_str(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag;
    off_t start_offset;


    /* checks */
    /* .vs: real extension (from .nam container)
     * .str: fake, partial header id */
    if (!check_extensions(sf, "vs,str"))
        goto fail;

    if (!(read_32bitBE(0x000,sf) == 0x5354524C &&   /* "STRL" */
          read_32bitBE(0x800,sf) == 0x53545252) &&  /* "STRR" */
        read_32bitBE(0x00,sf) != 0x5354524D)        /* "STRM" */
        goto fail;


    loop_flag = 0;
    channel_count = (read_32bitBE(0x00,sf) == 0x5354524D) ? 1 : 2; /* "STRM"=mono (voices) */
    start_offset = 0x00;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VS_STR;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_vs_str;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    /* calc num_samples */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += ps_bytes_to_samples(vgmstream->current_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        block_update(start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
