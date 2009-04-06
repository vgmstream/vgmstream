#include "meta.h"
#include "../util.h"

/* manakoAT 28.01.2009 :
	BAKA - found in "Crypt Killer (Saturn)...
    looks like some developers were really bored, every file starts with
    the word "BAKA" which is the japanese word for "IDIOT" :o)
    Files containing "begloop" markers at EOF...
	some files should loop, but i don't know how to get the loopstart here!*/
VGMSTREAM * init_vgmstream_sat_baka(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("baka",filename_extension(filename))) goto fail;

    /* check header */
    if ((read_32bitBE(0x00,streamFile) != 0x42414B41 &&  /* "BAKA" */
        read_32bitBE(0x08,streamFile) != 0x2041484F &&  /* " AHO" */
        read_32bitBE(0x0C,streamFile) != 0x50415041 &&  /* "PAPA" */
        read_32bitBE(0x26,streamFile) != 0x4D414D41))   /* "MAMA" */
    goto fail;

    
    channel_count = 2;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x2E;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->num_samples = read_32bitBE(0x16,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitBE(0x16,streamFile);
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_SAT_BAKA;

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
