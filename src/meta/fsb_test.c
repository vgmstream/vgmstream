#include "meta.h"
#include "../util.h"

/* FSB3.0 & FSB3.1*/
VGMSTREAM * init_vgmstream_fsb3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int fsb_headerlen;
  int loop_flag;
  int channel_count;
    off_t start_offset;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename)) &&
    strcasecmp("lfsb",filename_extension(filename)))
  goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x46534233) goto fail; /* "FSB3" */
  
  /* "Check if the FSB is used as	conatiner or as single file" */
  if (read_32bitLE(0x04,streamFile) != 0x1) goto fail;
    
  /* Check if we're dealing with a FSB3.0 or FSB3.1 file */
    if ((read_32bitBE(0x10,streamFile) != 0x00000300) && 
        (read_32bitBE(0x10,streamFile) != 0x01000300))
    goto fail;

  if (read_32bitBE(0x48,streamFile) == 0x02000806) { // Metroid Prime 3
        loop_flag = 1;
    } else {
        loop_flag = 0;
    }

        channel_count = read_16bitLE(0x56,streamFile);
        fsb_headerlen = read_32bitLE(0x08,streamFile);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
        if (!vgmstream) goto fail;

    // fsb_format = ;
  switch (((uint8_t)read_8bit(0x4A, streamFile)) >> 4) {
    case 0x0: // Nintendo DSP
    // Hack to support an illegal coding flag
    if (read_32bitBE(0x48,streamFile) == 0x50010000) { // Fantastic 4: Rise of the Silver Surfer
      vgmstream->coding_type = coding_PCM16LE;
      vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))/2/channel_count;
    if (loop_flag) {
      vgmstream->loop_start_sample = 0;
      vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))/2/channel_count;
    }
    if (channel_count == 1) {
      vgmstream->layout_type = layout_none;
    } else if (channel_count > 1) {
      vgmstream->layout_type = layout_interleave;
      vgmstream->interleave_block_size = 2;
    }
      } else {
      vgmstream->coding_type = coding_NGC_DSP;
      vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*14/8/channel_count;
    if (loop_flag) {
      vgmstream->loop_start_sample = 0;
      vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))*14/8/channel_count;
    }
    if (channel_count == 1) {
      vgmstream->layout_type = layout_none;
    } else if (channel_count > 1) {
      vgmstream->layout_type = layout_interleave_byte;
      vgmstream->interleave_block_size = 2;
    }
  }
    break;
    case 0x4: // XBOX IMA ADPCM
      vgmstream->coding_type = coding_XBOX;
      vgmstream->layout_type = layout_none;
      vgmstream->num_samples = read_32bitLE(0x0C,streamFile)*64/36/channel_count;
    if (loop_flag) {
      vgmstream->loop_start_sample = 0;
      vgmstream->loop_end_sample = read_32bitLE(0x0C,streamFile)*64/36/channel_count;
    }
    break;
    case 0x8: // PS2 APDCM
      vgmstream->coding_type = coding_PSX;
      vgmstream->layout_type = layout_interleave;
      vgmstream->interleave_block_size = 0x10;
      vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    if (loop_flag) {
      vgmstream->loop_start_sample = 0;
      vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    }
    break;
      default:
        goto fail;
  }

  /* fill in the vital statistics */
    start_offset = fsb_headerlen+0x18;
    vgmstream->sample_rate = (read_32bitLE(0x4C, streamFile));
    
    if (read_32bitBE(0x10,streamFile) == 0x00000300) {
        vgmstream->meta_type = meta_FSB3_0;
    } else if (read_32bitBE(0x10,streamFile) == 0x01000300) {
        vgmstream->meta_type = meta_FSB3_1;
    }

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i,c;
        for (c=0;c<channel_count;c++) {
            for (i=0;i<16;i++) {
                vgmstream->ch[c].adpcm_coef[i] =
                    read_16bitBE(0x68+c*0x2e +i*2,streamFile);
            }
        }
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
