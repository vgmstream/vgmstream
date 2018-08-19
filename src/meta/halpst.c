#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_halpst(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    int channel_count;
    int loop_flag = 0;
    int header_length = 0x80;

    int32_t samples_l,samples_r;
    int32_t start_sample = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("hps",filename_extension(filename))) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0,streamFile)!=0x2048414C || /* " HAL" */
            read_32bitBE(4,streamFile)!=0x50535400)         /* "PST\0" */
        goto fail;
    
    /* details */
    channel_count = read_32bitBE(0xc,streamFile);
    /*max_block = read_32bitBE(0x10,streamFile)/channel_count;*/

    if (channel_count > 2) {
        /* align the header length needed for the extra channels */
        header_length = 0x10+0x38*channel_count;
        header_length = (header_length+0x1f)/0x20*0x20;
    }

    /* yay for redundancy, gives us something to test */
    samples_l = dsp_nibbles_to_samples(read_32bitBE(0x18,streamFile))+1;
    {
        int i;
        for (i=1;i<channel_count;i++) {
            samples_r = dsp_nibbles_to_samples(read_32bitBE(0x18+0x38*i,streamFile))+1;
            if (samples_l != samples_r) goto fail;
        }
    }

    /*
     * looping info is implicit in the "next block" field of the final
     * block, so we have to find that
     */
    {
        off_t offset = header_length, last_offset = 0;
        off_t loop_offset;

        /* determine if there is a loop */
        while (offset > last_offset) {
            last_offset = offset;
            offset = read_32bitBE(offset+8,streamFile);
        }
        if (offset < 0) loop_flag = 0;
        else {
            /* one more pass to determine start sample */
            int32_t start_nibble = 0;
            loop_flag = 1;

            loop_offset = offset;
            offset = header_length;
            while (offset != loop_offset) {
                start_nibble += read_32bitBE(offset+4,streamFile)+1;
                offset = read_32bitBE(offset+8,streamFile);
            }

            start_sample = dsp_nibbles_to_samples(start_nibble);
        }

    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = samples_l;
    vgmstream->sample_rate = read_32bitBE(8,streamFile);
    /* channels and loop flag are set by allocate_vgmstream */
    if (loop_flag) {
        vgmstream->loop_start_sample = start_sample;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_halpst;
    vgmstream->meta_type = meta_HALPST;

    /* load decode coefs */
    {
        int i,j;
        for (i=0;i<channel_count;i++)
            for (j=0;j<16;j++)
                vgmstream->ch[i].adpcm_coef[j] = read_16bitBE(0x20+0x38*i+j*2,streamFile);
    }

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

    /* start me up */
    block_update_halpst(header_length,vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
