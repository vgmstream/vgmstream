#include "meta.h"
#include "../util.h"

/* FSB3.0 and FSB3.1 */

VGMSTREAM * init_vgmstream_fsb3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int fsb_headerlen;
    int channel_count;
    int loop_flag = 0;
  	int FSBFlag = 0;
    int i, c;
    off_t start_offset;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    
    if (strcasecmp("fsb",filename_extension(filename)))
  	    goto fail;

    /* check header for "FSB3" string */
    if (read_32bitBE(0x00,streamFile) != 0x46534233)
        goto fail;

  	/* "Check if the FSB is used as	conatiner or as single file" */
  	if (read_32bitLE(0x04,streamFile) != 0x1)
        goto fail;

  	/* Check if we're dealing with a FSB3.0 file */
    if ((read_32bitBE(0x10,streamFile) != 0x00000300) &&
        ((read_32bitBE(0x10,streamFile) != 0x01000300)))
    goto fail;

    channel_count = read_16bitLE(0x56,streamFile);
    fsb_headerlen = read_32bitLE(0x08,streamFile);

    FSBFlag = read_32bitLE(0x48,streamFile);
    
    if (FSBFlag&0x2 || FSBFlag&0x4 || FSBFlag&0x6)
      loop_flag = 1;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
        if (!vgmstream) goto fail;

    start_offset = fsb_headerlen+0x18;
    vgmstream->sample_rate = (read_32bitLE(0x4C, streamFile));
    
    
    // Get the Decoder
    if (FSBFlag&0x00000100)
    { // Ignore format and treat as RAW PCM
        vgmstream->coding_type = coding_PCM16LE;
        if (channel_count == 1)
        {
            vgmstream->layout_type = layout_none;
        }
        else if (channel_count > 1)
        {
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
        }
    }
    else if (FSBFlag&0x00400000)
    { // XBOX IMA
        vgmstream->coding_type = coding_XBOX;
        vgmstream->layout_type = layout_none;
    }
    else if (FSBFlag&0x00800000)
    { // PS2 ADPCM
        vgmstream->coding_type = coding_PSX;
        if (channel_count == 1)
        {
            vgmstream->layout_type = layout_none;
        }
        else if (channel_count > 1)
        {
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
        }
    }
    else if (FSBFlag&0x02000000)
    { // Nintendo DSP
				vgmstream->coding_type = coding_NGC_DSP;
        if (channel_count == 1)
        {
            vgmstream->layout_type = layout_none;
        }
        else if (channel_count > 1)
        {
            vgmstream->layout_type = layout_interleave_byte;
            vgmstream->interleave_block_size = 2;
        }
        // read coeff(s), DSP only
        for (c=0;c<channel_count;c++)
        {
            for (i=0;i<16;i++)
            {
                vgmstream->ch[c].adpcm_coef[i]=read_16bitBE(0x68+c*0x2e +i*2,streamFile);
            }
        }
    }


   vgmstream->num_samples = read_32bitLE(0x38,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x40,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(0x44,streamFile);
    }
    

    if (read_32bitBE(0x10,streamFile) == 0x00000300)
    {
      vgmstream->meta_type = meta_FSB3_0;
    }
    else if (read_32bitBE(0x10,streamFile) == 0x01000300)
    {
      vgmstream->meta_type = meta_FSB3_1;
    }
    
    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
            
            if (vgmstream->coding_type == coding_XBOX) {
                /* xbox interleaving is a little odd */
                vgmstream->ch[i].channel_start_offset=start_offset;
            } else {
                vgmstream->ch[i].channel_start_offset=
                    start_offset+vgmstream->interleave_block_size*i;
            }
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
