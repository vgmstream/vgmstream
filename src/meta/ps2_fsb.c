#include "meta.h"
#include "../util.h"

/* FSB  - FlatOut (XBOX), Guitar Hero III (WII), FlatOut 1 & 2 (PS2) */
VGMSTREAM * init_vgmstream_fsb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int fsb3_headerlen = 0x18;
    int fsb3_format;
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x46534233) /* "FSB3\0" */
        goto fail;

    loop_flag = 0; /* (read_32bitLE(0x08,streamFile)!=0); */
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* This will be tricky ;o) */
	fsb3_format = read_32bitBE(0x48,streamFile);
	switch (fsb3_format) {
		case 0x40008800: /* PS2 (Agent Hugo, Flat Out 2) */
		vgmstream->coding_type = coding_PSX;
		vgmstream->layout_type = layout_interleave;
		vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    }
	break;
		case 0x41008800: /* PS2 (Flat Out) */
		vgmstream->coding_type = coding_PSX;
		vgmstream->layout_type = layout_interleave;
		vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    }

	break;
		case 0x02000806: /* WII (Metroid Prime) */
		vgmstream->coding_type = coding_NGC_DSP;
		vgmstream->layout_type = layout_interleave_byte;
	break;
		case 0x40000802: /* WII () */
		vgmstream->coding_type = coding_NGC_DSP;
		vgmstream->layout_type = layout_interleave;
		vgmstream->num_samples = (read_32bitLE(0x0C,streamFile));
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile));
    }
	break;
        default:
			goto fail;
	}
	/* fill in the vital statistics */
  start_offset = (read_32bitLE(0x08,streamFile))+fsb3_headerlen;
	vgmstream->channels = read_16bitLE(0x56,streamFile);
    vgmstream->sample_rate = read_32bitLE(0x4C,streamFile);
    


    
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_FSB;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
