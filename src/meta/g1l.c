#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

static VGMSTREAM * init_vgmstream_kt_wiibgm_offset(STREAMFILE *streamFile, off_t offset);

/* Koei Tecmo G1L - pack format, sometimes containing a single stream
 *
 * It probably makes more sense to extract it externally, it's here mainly for Hyrule Warriors */
VGMSTREAM * init_vgmstream_kt_g1l(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	int type, num_streams, target_stream = 1;
	off_t stream_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;

	if (!check_extensions(streamFile,"g1l"))
		goto fail;

	/* check header */
	if ((read_32bitBE(0x0, streamFile) != 0x47314C5F            /* "G1L_" (BE) */
	        || read_32bitBE(0x0, streamFile) != 0x5F4C3147)     /* "_L1G" (LE) */
	        && read_32bitBE(0x4, streamFile) != 0x30303030)     /* "0000" (version?) */
		goto fail;

	if (read_32bitBE(0x0, streamFile) == 0x47314C5F ) {
	    read_32bit = read_32bitBE;
	} else {
        read_32bit = read_32bitLE;
	}


    /* 0x08 filesize */
    /* 0x0c first file offset (same as 0x18) */
	type = read_32bit(0x10,streamFile);
	num_streams = read_32bit(0x14,streamFile);
    if (target_stream==0) target_stream = 1;
	if (target_stream < 0 || target_stream > num_streams || num_streams < 1) goto fail;

    stream_offset = read_32bit(0x18 + 0x4*(target_stream-1),streamFile);
    /* filesize = stream_offset - stream_next_offset*/

    switch(type) { /* type may not be correct */
        case 0x09: /* DSP (WiiBGM) from Hyrule Warriors (Wii U) */
            vgmstream = init_vgmstream_kt_wiibgm_offset(streamFile, stream_offset);
            break;
        case 0x01: /* ATRAC3plus (RIFF) from One Piece Pirate Warriors 2 (PS3) */
        case 0x00: /* OGG (KOVS) from Romance Three Kindgoms 13 (PC)*/
        case 0x0A: /* OGG (KOVS) from Dragon Quest Heroes (PC)*/
        default:
            goto fail;
    }


	return vgmstream;
fail:
	close_vgmstream(vgmstream);
	return NULL;
}

/* Koei Tecmo "WiiBGM" DSP format - found in Hyrule Warriors, Romance of the Three Kingdoms 12 */
VGMSTREAM * init_vgmstream_kt_wiibgm(STREAMFILE *streamFile) {
    return init_vgmstream_kt_wiibgm_offset(streamFile, 0x0);
}

static VGMSTREAM * init_vgmstream_kt_wiibgm_offset(STREAMFILE *streamFile, off_t offset) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    off_t start_offset;

    if (!check_extensions(streamFile,"g1l,dsp"))
        goto fail;

    if (read_32bitBE(offset+0x0, streamFile) != 0x57696942 && /* "WiiB" */
        read_32bitBE(offset+0x4, streamFile) != 0x474D0000)   /* "GM\0\0" */
        goto fail;

    /* check type details */
    loop_flag = read_32bitBE(offset+0x14, streamFile) > 0;
    channel_count = read_8bit(offset+0x23, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(offset+0x10, streamFile);
    vgmstream->sample_rate = (uint16_t)read_16bitBE(offset+0x26, streamFile);
    vgmstream->loop_start_sample = read_32bitBE(offset+0x14, streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave_byte;
    vgmstream->meta_type = meta_KT_WIIBGM;

    vgmstream->interleave_block_size = 0x1;

    dsp_read_coefs_be(vgmstream,streamFile, offset+0x5C, 0x60);
    start_offset = offset+0x800;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

    fail:
        close_vgmstream(vgmstream);
        return NULL;
}
