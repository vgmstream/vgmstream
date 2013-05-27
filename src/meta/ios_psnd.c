#include "meta.h"
#include "../util.h"

/* PSND (from Crash Bandicoot Nitro Kart 2 (iOS) */
VGMSTREAM * init_vgmstream_ios_psnd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("psnd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x50534E44) /* "PSND" */
        goto fail;

    if (read_16bitBE(0xC,streamFile)==0x2256){
		loop_flag = 1;
	}
	else {
		loop_flag = 0;
	}
	channel_count = read_8bit(0xE,streamFile);

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0x10;
    vgmstream->channels = channel_count;
	
	if (read_16bitBE(0xC,streamFile)==0x44AC){
		vgmstream->sample_rate = 44100;
	}
	else {
        vgmstream->sample_rate = read_16bitLE(0xC,streamFile);
	}

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (read_32bitLE(0x4,streamFile)-8)/4;
	if (loop_flag) {
        vgmstream->loop_start_sample = 0;
       vgmstream->loop_end_sample = vgmstream->num_samples;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 2;
    vgmstream->meta_type = meta_IOS_PSND;

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
