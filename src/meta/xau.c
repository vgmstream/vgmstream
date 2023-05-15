#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include "../util/chunks.h"

/* XAU - XPEC Entertainment sound format [Beat Down (PS2/Xbox), Spectral Force Chronicle (PS2)] */
VGMSTREAM* init_vgmstream_xau(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, type, loop_start, loop_end;


    /* check extension */
    if (!is_id32be(0x00,sf, "XAU\0"))
        goto fail;

    if (!check_extensions(sf, "xau"))
        goto fail;

    if (read_32bitLE(0x08,sf) != 0x40) /* header start */
        goto fail;

    /* 0x04: version? (0x100) */
    type       = read_32bitBE(0x0c, sf);
    loop_start = read_32bitLE(0x10, sf);
    loop_end   = read_32bitLE(0x14, sf);
    loop_flag  = (loop_end > 0);

    channels = read_8bit(0x18,sf);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = channels;
    vgmstream->meta_type = meta_XAU;

    /* miniheader over a common header with some tweaks, so we'll simplify parsing */
    switch(type) {
        case 0x50533200: /* "PS2\0" */
            if (read_32bitBE(0x40,sf) != 0x56414770) goto fail; /* mutant "VAGp" (long header size) */

            start_offset = 0x800;
            vgmstream->sample_rate = read_32bitBE(0x50, sf);
            vgmstream->num_samples = ps_bytes_to_samples(read_32bitBE(0x4C,sf) * channels, channels);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;
            break;

        case 0x58420000: /* "XB\0\0" */
            if (read_32bitBE(0x40,sf) != 0x52494646) goto fail; /* mutant "RIFF" (sometimes wrong RIFF size) */

            /* start offset: find "data" chunk, as sometimes there is a "smpl" chunk at the start or the end (same as loop_start/end) */
            if (!find_chunk_le(sf, 0x64617461, 0x4c, 0, &start_offset, NULL) )
                goto fail;

            vgmstream->sample_rate = read_32bitLE(0x58, sf);
            vgmstream->num_samples = xbox_ima_bytes_to_samples(read_32bitLE(start_offset-4, sf), channels);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
