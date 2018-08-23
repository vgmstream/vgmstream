#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* a few formats from Excitebots */

/* .sfx, some .sf0 -  DSP and PCM */
VGMSTREAM * init_vgmstream_eb_sfx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;
    int coding_type;

	long body_size;
	long header_size;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sfx",filename_extension(filename)) &&
        strcasecmp("sf0",filename_extension(filename)))
        goto fail;

    /* check sizes */
    body_size = read_32bitLE(0x00,streamFile);
    header_size = read_32bitLE(0x04,streamFile);

    if (body_size + header_size != get_streamfile_size(streamFile))
        goto fail;
    
    loop_flag = 0;

    switch (read_8bit(0x09,streamFile))
    {
        case 0:
            if (header_size != 0x20)
                goto fail;
            coding_type = coding_PCM16BE;
            break;
        case 1:
            if (header_size != 0x80)
                goto fail;
            coding_type = coding_NGC_DSP;
            loop_flag = 1;
            break;
        default:
            goto fail;
    }

    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = header_size;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_type;

    if (coding_NGC_DSP == coding_type)
    {
        vgmstream->num_samples = dsp_nibbles_to_samples(body_size*2);

        if (loop_flag)
        {
            vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bitBE(0x30,streamFile));
            vgmstream->loop_end_sample = dsp_nibbles_to_samples(read_32bitBE(0x34,streamFile));
        }
    }
    else
    {
        vgmstream->num_samples = body_size / 2;

        if (loop_flag)
        {
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_EB_SFX;
    vgmstream->allow_dual_stereo = 1;

    /* open the file for reading */
    {
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        vgmstream->ch[0].streamfile = file;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;

        if (coding_NGC_DSP == coding_type)
        {
            int i;
            for (i = 0; i < 16; i++)
            {
                vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x3C+i*2,streamFile);
            }
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* .sf0 - PCM (degenerate stereo .sfx?) */
VGMSTREAM * init_vgmstream_eb_sf0(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int loop_flag = 0;
	int channel_count;
    long file_size;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sf0",filename_extension(filename)))
        goto fail;

    /* no header, check file size and go on faith */
    file_size = get_streamfile_size(streamFile);
    if (file_size % 0x8000)
        goto fail;

    channel_count = 2;
    loop_flag = 0;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    vgmstream->sample_rate = 32000;
    vgmstream->num_samples = file_size / 4;
    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_EB_SF0;
    vgmstream->interleave_block_size = 0x4000;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset = vgmstream->interleave_block_size*i;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* don't know what to do about .sng and .sn0 */
