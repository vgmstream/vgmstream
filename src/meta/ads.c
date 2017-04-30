#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* ADS - from Gauntlet Dark Legacy (GC/Xbox) */
VGMSTREAM * init_vgmstream_ads(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag;
    int channel_count;
    int identifer_byte;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"ads")) goto fail;

    /* check dhSS Header */
    if (read_32bitBE(0x00,streamFile) != 0x64685353)
        goto fail;

    /* check dbSS Header */
    if (read_32bitBE(0x20,streamFile) != 0x64625353)
        goto fail;
    
    loop_flag = 1;
    channel_count = read_32bitBE(0x10,streamFile);

    if (channel_count > 0x2)
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    identifer_byte = read_32bitBE(0x08,streamFile);
    switch (identifer_byte) {
        case 0x00000020: /* GC */
            start_offset = 0x28 + 0x60 * channel_count;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = read_32bitBE(0x0c,streamFile);
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->num_samples = read_32bitBE(0x28,streamFile);
            if (loop_flag) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
        
            if (channel_count == 1){
                vgmstream->layout_type = layout_none;
            } else if (channel_count == 2){
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = read_32bitBE(0x14,streamFile);
            }

            dsp_read_coefs_be(vgmstream, streamFile, 0x44,0x60);
            break;

        case 0x00000021: /* Xbox */
            start_offset = 0x28;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = read_32bitBE(0x0c,streamFile);
            vgmstream->coding_type = coding_XBOX_int;
            vgmstream->num_samples = (read_32bitBE(0x24,streamFile) / 36 *64 / vgmstream->channels)-64; // to avoid the "pop" at the loop point
            vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 0x24;
            if (loop_flag) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            break;

        default:
            goto fail;
    }

    vgmstream->meta_type = meta_ADS;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
