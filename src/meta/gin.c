#include "meta.h"
#include "../coding/coding.h"

VGMSTREAM * init_vgmstream_gin_header(STREAMFILE *streamFile, off_t offset);

/* .gin - EA engine sounds [Need for Speed: Most Wanted (multi)] */
VGMSTREAM * init_vgmstream_gin(STREAMFILE *streamFile) {
    if (!check_extensions(streamFile, "gin"))
        goto fail;

    return init_vgmstream_gin_header(streamFile, 0x00);

fail:
    return NULL;
}

VGMSTREAM * init_vgmstream_gin_header(STREAMFILE *streamFile, off_t offset) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, num_samples;

    /* checks */
    if (read_32bitBE(offset + 0x00, streamFile) != 0x476E7375) /* "Gnsu" */
        goto fail;

    /* contains mapped values for engine RPM sounds but we'll just play the whole thing */
    /* 0x04: size? "20\00\00"? */
    /* 0x08/0c: min/max float RPM? */
    /* 0x10: RPM up? (pitch/frequency) table size */
    /* 0x14: RPM ??? table size */
    /* always LE even on X360/PS3 */

    num_samples = read_32bitLE(offset + 0x18, streamFile);
    sample_rate = read_32bitLE(offset + 0x1c, streamFile);
    start_offset = offset + 0x20 +
        (read_32bitLE(offset + 0x10, streamFile) + 1) * 0x04 +
        (read_32bitLE(offset + 0x14, streamFile) + 1) * 0x04;
    channel_count = 1;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GIN;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_EA_XAS_V0;
    vgmstream->layout_type = layout_none;

    /* calculate size for TMX */
    vgmstream->stream_size = (align_size_to_block(num_samples, 32) / 32) * 0x13;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
