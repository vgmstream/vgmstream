#include "meta.h"
#include "../coding/coding.h"

/* SDF - from Beyond Reality games */
VGMSTREAM * init_vgmstream_sdf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count, sample_rate, interleave, coefs_offset;


    /* checks */
    if (!check_extensions(streamFile,"sdf"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53444600) /* "SDF\0" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x03000000) /* version? */
        goto fail;

    data_size = read_32bitLE(0x08,streamFile);
    start_offset = get_streamfile_size(streamFile) - data_size;

    switch(start_offset) {
        case 0x18: /* Agent Hugo - Lemoon Twist (PS2)*/
            sample_rate     = read_32bitLE(0x0c,streamFile);
            channel_count   = read_32bitLE(0x10,streamFile);
            interleave      = read_32bitLE(0x14,streamFile);
            break;

        case 0x78: /* Gummy Bears Mini Golf (3DS) */
            sample_rate     = read_32bitLE(0x10,streamFile);
            channel_count   = read_32bitLE(0x14,streamFile);
            interleave      = read_32bitLE(0x18,streamFile);
            coefs_offset    = 0x1c;
            break;

        case 0x84: /* Mr. Bean's Wacky World (Wii) */
            sample_rate     = read_32bitLE(0x10,streamFile);
            channel_count   = read_32bitLE(0x14,streamFile);
            interleave      = read_32bitLE(0x18,streamFile);
            data_size       = read_32bitLE(0x20,streamFile); /* usable size */
            coefs_offset    = 0x28;
            break;

        default:
            goto fail;
    }

    loop_flag = 1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDF;
    vgmstream->sample_rate = sample_rate;

    switch(start_offset) {
        case 0x18:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count);
            break;

        case 0x78:
        case 0x84:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            if (vgmstream->interleave_block_size == 0) /* Gummy Bears Mini Golf */
                vgmstream->interleave_block_size = data_size / channel_count;

            vgmstream->num_samples = dsp_bytes_to_samples(data_size,channel_count);

            dsp_read_coefs_le(vgmstream, streamFile, coefs_offset+0x00,0x2e);
            dsp_read_hist_le (vgmstream, streamFile, coefs_offset+0x24,0x2e);
            break;

        default:
            goto fail;
    }

    /* most songs simply repeat; don't loop if too short (in seconds) */
    if (vgmstream->num_samples > 10*sample_rate) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
