#include "meta.h"
#include "../coding/coding.h"

/* SVAG - from Konami Tokyo games [OZ (PS2), Neo Contra (PS2), Silent Hill 2 (PS2)] */
VGMSTREAM* init_vgmstream_svag_kcet(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(sf, "svag"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x53766167) /* "Svag" */
        goto fail;

    channel_count = read_16bitLE(0x0C,sf); /* always 2? ("S"tereo vag?) */
    loop_flag = (read_32bitLE(0x14,sf) ==1);

    /* test padding (a set "KCE-Tokyo ..." phrase) after 0x1c to catch bad rips,
     * at 0x400 may be header  again (Silent Hill 2) or more padding (Silent Scope 2) */
    if (channel_count > 1 && 
        read_32bitBE(0x400,sf) != 0x53766167 &&     /* "Svag" */
        read_32bitBE(0x400,sf) != 0x44657369)       /* "Desi" */
        goto fail;

    start_offset = 0x800;
    data_size = read_32bitLE(0x04,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,sf);

    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x04,sf), vgmstream->channels);
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x18,sf)*vgmstream->channels, vgmstream->channels);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_SVAG_KCET;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x10,sf);
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
