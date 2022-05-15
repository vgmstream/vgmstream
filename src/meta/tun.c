#include "meta.h"
#include "../coding/coding.h"

/* ALP - from LEGO Racers (PC) */
VGMSTREAM* init_vgmstream_tun(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "ALP "))
        goto fail;
    if (!check_extensions(sf,"tun"))
        goto fail;
    /* also "ADPCM" at 0x08 */

    channels = 2; /* probably at 0x0F */
    loop_flag = 0;
    start_offset = 0x10;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_TUN;
    vgmstream->channels = channels;
    vgmstream->sample_rate = 22050;
    vgmstream->num_samples = ima_bytes_to_samples(get_streamfile_size(sf) - 0x10, channels);

    vgmstream->coding_type = coding_HV_IMA;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
