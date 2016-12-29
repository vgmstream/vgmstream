#include "meta.h"
#include "../util.h"
#include "../header.h"

/* BGW (FF XI)
 *
 * Some info from POLUtils
 */
VGMSTREAM * init_vgmstream_bgw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    uint32_t codec, filesize, blocksize, sample_rate;
    int32_t loop_start;
    uint8_t block_align;
    off_t start_offset;

    int loop_flag = 0;
	int channel_count;

    /* check extensions */
    if ( !header_check_extensions(streamFile, "bgw") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x42474d53 || /* "BGMS" */
        read_32bitBE(0x04,streamFile) != 0x74726561 || /* "trea" */
        read_32bitBE(0x08,streamFile) != 0x6d000000 )  /* "m\0\0\0" */
        goto fail;

    codec = read_32bitLE(0x0c,streamFile);
    filesize = read_32bitLE(0x10,streamFile);
    /*file_id = read_32bitLE(0x14,streamFile);*/
    blocksize = read_32bitLE(0x18,streamFile);
    loop_start = read_32bitLE(0x1c,streamFile);
    sample_rate = (read_32bitLE(0x20,streamFile) + read_32bitLE(0x24,streamFile)) & 0xFFFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_32bitLE(0x28,streamFile);
    /*0x2c: unk (vol?) */
    /*0x2d: unk (0x10?) */
    channel_count = read_8bit(0x2e,streamFile);
    block_align = read_8bit(0x2f,streamFile);


    /* check file size with header value */
    if (filesize != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (loop_start > 0);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    vgmstream->meta_type = meta_FFXI_BGW;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = blocksize * block_align;
    if (loop_flag) {
        vgmstream->loop_start_sample = (loop_start-1) * block_align;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    switch (codec) {
        case 0: { /* ADPCM */
            vgmstream->coding_type = coding_VAG_ADPCM_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */
            break;
        }
        case 1: /* PCM */
        case 3: /* ATRAC3 (encrypted) */
        default:
            goto fail;
    }



    /* open the file for reading */
    if ( !header_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .spw (SEWave, PlayOnline viewer for FFXI), very similar to bgw  */
VGMSTREAM * init_vgmstream_spw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;

    int loop_flag = 0;
    int32_t loop_start;
	int channel_count;

    /* check extension, case insensitive */
    if ( !header_check_extensions(streamFile, "spw") )
        goto fail;

    /* "SeWave" */
    if (read_32bitBE(0,streamFile) != 0x53655761 ||
        read_32bitBE(4,streamFile) != 0x76650000) goto fail;

    /* check file size with header value */
    if (read_32bitLE(0x8,streamFile) != get_streamfile_size(streamFile))
        goto fail;

    channel_count = read_8bit(0x2a,streamFile);
    loop_start = read_32bitLE(0x18,streamFile);
    loop_flag = (loop_start > 0);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = read_32bitLE(0x24,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_VAG_ADPCM_cfg;
    vgmstream->num_samples = read_32bitLE(0x14,streamFile)*16;
    if (loop_flag) {
        vgmstream->loop_start_sample = (loop_start-1)*16;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 9;
    vgmstream->meta_type = meta_FFXI_SPW;


    /* open the file for reading */
    if ( !header_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
