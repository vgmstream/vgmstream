#include "meta.h"
#include "../util.h"

/* RAS (from Donkey Kong Country Returns) */
VGMSTREAM * init_vgmstream_wii_ras(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ras",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x5241535F) /* "RAS_" */
        goto fail;

    loop_flag = 0;
    if (read_32bitBE(0x30,streamFile) != 0 ||
        read_32bitBE(0x34,streamFile) != 0 ||
        read_32bitBE(0x38,streamFile) != 0 ||
        read_32bitBE(0x3C,streamFile) != 0) {
        loop_flag = 1;
    }
    channel_count = 2;

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = read_32bitBE(0x18,streamFile);
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x14,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x1c,streamFile)/channel_count/8*14;
    vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = read_32bitBE(0x20,streamFile);
    vgmstream->meta_type = meta_WII_RAS;

    if (loop_flag) {
        // loop is block + samples into block
        vgmstream->loop_start_sample = 
        read_32bitBE(0x30,streamFile)*vgmstream->interleave_block_size/8*14 + 
            read_32bitBE(0x34,streamFile);
        vgmstream->loop_end_sample =
        read_32bitBE(0x38,streamFile)*vgmstream->interleave_block_size/8*14 +
            read_32bitBE(0x3C,streamFile);
    }

	 if (vgmstream->coding_type == coding_NGC_DSP) {
         int i;
         for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x40+i*2,streamFile);
        }
	if (channel_count == 2) {
		  for (i=0;i<16;i++)
			vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x70+i*2,streamFile);
				}
    } else {
        goto fail;
    }

    /* open the file for reading */
   {
        int i;
        for (i=0;i<channel_count;i++) {
            if (vgmstream->layout_type==layout_interleave_shortblock)
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,
                    vgmstream->interleave_block_size);
            else
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,
                    0x1000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                start_offset + i*vgmstream->interleave_block_size;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
