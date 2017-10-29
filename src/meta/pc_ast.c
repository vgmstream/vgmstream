#include "meta.h"
#include "../coding/coding.h"

/* ASTL - found in Dead Rising (PC) */
VGMSTREAM * init_vgmstream_pc_ast(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
	off_t start_offset, data_size;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"ast"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x4153544C) /* "ASTL" */
        goto fail;


    loop_flag = 0; //TODO - Find hidden loop point calc and flag
	channel_count = read_8bit(0x32, streamFile);
	data_size = read_32bitLE(0x20,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* TODO - Find non-obvious loop points and flag (if any) */
    start_offset = read_32bitLE(0x10,streamFile);
    vgmstream->sample_rate = read_32bitLE(0x34,streamFile);
	vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = data_size/(channel_count*2);
	vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_PC_AST;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
