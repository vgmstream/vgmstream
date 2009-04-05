#include "meta.h"
#include "../util.h"

/* 28.01.2009 - bxaimc :
    SWAV - found in Asphalt Urban GT & Asphalt Urban GT 2 */
VGMSTREAM * init_vgmstream_nds_swav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int codec_number;
    int channel_count;
    int loop_flag;
	coding_t coding_type;
    off_t start_offset;
    int bits_per_sample;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("swav",filename_extension(filename))) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0x00,streamFile)!=0x53574156)	/* SWAV */
        goto fail;

    /* check for DATA section */
    if ((uint32_t)read_32bitBE(0x10,streamFile)!=0x44415441) /* "DATA" */
        goto fail;

    /* check type details */
    codec_number = read_8bit(0x18,streamFile);
    loop_flag = read_8bit(0x19,streamFile);

    channel_count = 1;
    if (get_streamfile_size(streamFile) != read_32bitLE(0x8,streamFile))
    {
        if (get_streamfile_size(streamFile) !=
                (read_32bitLE(0x8,streamFile) - 0x24) * 2 + 0x24)
            goto fail;

        channel_count = 2;
    }

	switch (codec_number) {
        case 0:
            coding_type = coding_PCM8;
            bits_per_sample = 8;
            break;
        case 1:
            coding_type = coding_PCM16LE;
            bits_per_sample = 16;
            break;
        case 2:
            coding_type = coding_INT_IMA;
            bits_per_sample = 4;
            break;
        default:
            goto fail;
    }

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x24;
	vgmstream->num_samples =
        (read_32bitLE(0x14,streamFile) - 0x14) * 8 / bits_per_sample;
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x1A,streamFile);

	if (loop_flag) {
		vgmstream->loop_start_sample =
            (uint16_t)read_16bitLE(0x1E,streamFile) * 32 / bits_per_sample;
		vgmstream->loop_end_sample =
            read_32bitLE(0x20,streamFile) * 32 / bits_per_sample +
            vgmstream->loop_start_sample;
	}

    if (coding_type == coding_INT_IMA) {
        /* handle IMA frame header */
        vgmstream->loop_start_sample -= 32 / bits_per_sample;
        vgmstream->loop_end_sample -= 32 / bits_per_sample;
        vgmstream->num_samples -= 32 / bits_per_sample;

        {
            int i;
            for (i=0; i<channel_count; i++) {
                vgmstream->ch[i].adpcm_history1_32 =
                    read_16bitLE(start_offset + 0 + 4*i, streamFile);
                vgmstream->ch[i].adpcm_step_index =
                    read_16bitLE(start_offset + 2 + 4*i, streamFile);
            }
        }

        start_offset += 4 * channel_count;
    }
	
	vgmstream->coding_type = coding_type;
    vgmstream->meta_type = meta_NDS_SWAV;
    if (channel_count == 2) {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 1;
    } else {
        vgmstream->layout_type = layout_none;
    }

    /* open the file for reading by each channel */
      
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
