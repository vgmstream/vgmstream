#include "meta.h"
#include "../coding/coding.h"

/* SL3 - Sirens Sound Library (Winky Soft / Atari Melbourne House) games [Test Drive Unlimited (PS2), Transformers 2003/2004 (PS2)] */
VGMSTREAM* init_vgmstream_sl3(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "SL3\0"))
        goto fail;
    /* .ms: actual extension */
    if (!check_extensions(sf, "ms"))
        goto fail;

    loop_flag = 0;
    channels = read_32bitLE(0x14,sf);
    start_offset = 0x8000; /* also at 0x24? */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SL3;
    vgmstream->sample_rate = read_32bitLE(0x18,sf);
    vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(sf)-start_offset,channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x20,sf);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
