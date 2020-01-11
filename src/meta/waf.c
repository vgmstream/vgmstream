#include "meta.h"
#include "../coding/coding.h"

/* WAF - KID's earlier PC games [ever17 (PC)] (for RLE-compressed WAFs see https://github.com/dsp2003/e17p) */
VGMSTREAM * init_vgmstream_waf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "waf"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x57414600) /* "WAF\0" */
        goto fail;
    if (read_32bitLE(0x34,streamFile) + 0x38 != get_streamfile_size(streamFile))
        goto fail;

    channel_count = read_16bitLE(0x06,streamFile);
    loop_flag  = 0;
    start_offset = 0x38;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WAF;
    vgmstream->sample_rate = read_32bitLE(0x08, streamFile);

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->frame_size = read_16bitLE(0x10, streamFile);
    /* 0x04: null?, 0x0c: avg br, 0x12: bps, 0x14: s_p_f, 0x16~34: coefs (a modified RIFF fmt) */
    if (!msadpcm_check_coefs(streamFile, 0x16))
        goto fail;

    vgmstream->num_samples = msadpcm_bytes_to_samples(read_32bitLE(0x34,streamFile), vgmstream->frame_size, channel_count);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
