#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* Guerrilla's MSS - Found in ShellShock Nam '67 (PS2/Xbox), Killzone (PS2) */
VGMSTREAM * init_vgmstream_mss(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	off_t start_offset;
	size_t data_size;
	int loop_flag = 0, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "mss"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4D435353) /* "MCSS" */
        goto fail;

    loop_flag = 0;
    channel_count = read_16bitLE(0x16,streamFile);

    /* 0x04: version? (always 0x00000100 LE) */
    start_offset = read_32bitLE(0x08,streamFile);
    data_size = read_32bitLE(0x0c,streamFile);


	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
	/* 0x14(1): 1/2/3/4 if 2/4/6/8ch,  0x15(1): 0/1?,  0x16: ch */
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x18,streamFile);
    vgmstream->num_samples = read_32bitLE(0x1C,streamFile);
    vgmstream->meta_type = meta_MSS;

    /* no other way to know */
    if (vgmstream->interleave_block_size == 0x4800) {
        vgmstream->coding_type = coding_XBOX_IMA;

        /* in stereo multichannel this value is distance between 2ch pair, but we need
         * interleave*ch = full block (2ch 0x4800 + 2ch 0x4800 = 4ch, 0x4800+4800 / 4 = 0x2400) */
        vgmstream->interleave_block_size = vgmstream->interleave_block_size / 2;
        if (vgmstream->channels > 2 && vgmstream->channels % 2 != 0)
            goto fail; /* only 2ch+..+2ch layout is known */

        /* header values are somehow off? */
        data_size = get_streamfile_size(streamFile) - start_offset;
        vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
    }
    else {
        /* 0x800 interleave */
        vgmstream->coding_type = coding_PSX;

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
