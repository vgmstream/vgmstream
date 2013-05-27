#include "meta.h"
#include "../util.h"

/* TEC (from TECMO games) */
/* probably TECMO Vag Stream */
VGMSTREAM * init_vgmstream_ps2_tec(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
  	int loop_flag;
	  int channel_count;
    int current_chunk;
    off_t start_offset;
    int dataBuffer = 0;
    int Founddata = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("tec",filename_extension(filename))) goto fail;

    loop_flag = 0;
    channel_count = 2;
    
	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
	  start_offset = 0x0;
	  vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX_badflags;
    vgmstream->num_samples = get_streamfile_size(streamFile)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = get_streamfile_size(streamFile)*28/16/channel_count;
    }

    // Check the first frame header (should be always zero)
    if ((uint8_t)(read_8bit(0x00,streamFile) != 0x0))
        goto fail;

    // Scan for Interleave
    {
        current_chunk = 16;
        while (!Founddata && current_chunk < 65536) {
        dataBuffer = (uint8_t)(read_8bit(current_chunk,streamFile));
            if (dataBuffer == 0x0) { /* "0x0" */
                Founddata = 1;
                break;
            }
            current_chunk = current_chunk + 16;
        }
    }

    // Cancel if we can't find an interleave
    if (Founddata == 0) {
        goto fail;
    } else if (Founddata == 1) {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = current_chunk;
    }
    
    // Cancel if the first flag isn't invalid/bad
    if ((uint8_t)(read_8bit(0x01,streamFile) == 0x0))
        goto fail;
    if ((uint8_t)(read_8bit(0x01+current_chunk,streamFile) == 0x0))
        goto fail;

    vgmstream->meta_type = meta_PS2_TEC;
    
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
