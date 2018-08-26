#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* HWAS - found in Vicarious Visions NDS games (Spider-Man 3, Tony Hawk's Downhill Jam, etc) */
VGMSTREAM * init_vgmstream_nds_hwas(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag = 0;

    /* checks */
    /* .hwas: usually in archives but also found named (ex. Guitar Hero On Tour) */
    if (!check_extensions(streamFile,"hwas"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x73617768) /* "sawh" */
        goto fail;

    loop_flag = 1; /* almost all files seem to loop (usually if num_samples != loop_end doesn't loop, but not always) */
    channel_count = read_32bitLE(0x0C,streamFile);
    if (channel_count > 1) goto fail; /* unknown block layout when stereo, not seen */

    start_offset = 0x200;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NDS_HWAS;
    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = ima_bytes_to_samples(read_32bitLE(0x14,streamFile), channel_count);
    vgmstream->loop_start_sample = ima_bytes_to_samples(read_32bitLE(0x10,streamFile), channel_count); //assumed, always 0
    vgmstream->loop_end_sample = ima_bytes_to_samples(read_32bitLE(0x18,streamFile), channel_count);

    vgmstream->coding_type = coding_IMA_int;
    vgmstream->layout_type = layout_blocked_hwas;
    vgmstream->full_block_size = read_32bitLE(0x04,streamFile); /* usually 0x2000, 0x4000 or 0x8000 */

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
