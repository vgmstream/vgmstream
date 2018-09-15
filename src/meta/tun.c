#include "meta.h"
#include "../coding/coding.h"

/* ALP - from LEGO Racers (PC) */
VGMSTREAM * init_vgmstream_tun(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    if ( !check_extensions(streamFile,"tun") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x414C5020) /* "ALP " */
        goto fail;

    channel_count = 2; /* probably at 0x0F */
    loop_flag = 0;
    start_offset = 0x10;
    /* also "ADPCM" at 0x08 */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 22050;
    vgmstream->num_samples = ima_bytes_to_samples(get_streamfile_size(streamFile) - 0x10, channel_count);

    vgmstream->coding_type = coding_ALP_IMA;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->meta_type = meta_TUN;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
