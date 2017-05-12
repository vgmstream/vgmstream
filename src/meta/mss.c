#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* Guerrilla's MSS - Found in ShellShock Nam '67 (PS2/Xbox), Killzone (PS2) */
VGMSTREAM * init_vgmstream_mss(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	off_t start_offset;
	size_t data_size;
	int loop_flag = 0, channel_count;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile, "mss")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D435353) /* "MCSS" */
        goto fail;

    loop_flag = 0;
    channel_count = read_16bitLE(0x16,streamFile);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    /* 0x04: version? (always 0x00000100 LE) */
	start_offset = read_32bitLE(0x08,streamFile);
	data_size = read_32bitLE(0x0c,streamFile);
	vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
	/* 0x14(1): 1/2/3/4 if 2/4/6/8ch,  0x15(1): 0/1?,  0x16: ch */
    vgmstream->interleave_block_size = read_32bitLE(0x18,streamFile);
    vgmstream->num_samples = read_32bitLE(0x1C,streamFile);
    vgmstream->meta_type = meta_MSS;

    /* no other way to know */
    if (vgmstream->interleave_block_size == 0x4800) {
        /* interleaved stereo streams (2ch 0x4800 + 2ch 0x4800 = 4ch) */
        vgmstream->coding_type = coding_XBOX;
        vgmstream->layout_type = layout_interleave;

        /* header values are somehow off? */
        data_size = get_streamfile_size(streamFile);
        vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, 0x24*vgmstream->channels, vgmstream->channels);

        vgmstream->channels = 2; //todo add support for interleave stereo streams
    }
    else {
        /* 0x800 interleave */
        vgmstream->coding_type = coding_PSX;
        vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;

        if (vgmstream->num_samples * vgmstream->channels <= data_size)
            vgmstream->num_samples = vgmstream->num_samples / 16 * 28;
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
