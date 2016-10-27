#include "meta.h"
#include "../util.h"

/* PS2 SVAG (SNK)
 *
 * Found in SNK's World Heroes Anthology and Fatal Fury Battle Archives 2, maybe others
 * No relation with Konami's SVAG.
 */

VGMSTREAM * init_vgmstream_ps2_svag_snk(STREAMFILE* streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("svag",filename_extension(filename))) goto fail;


    /* check SNK SVAG Header ("VAGm") */
    if (read_32bitBE(0x00,streamFile) != 0x5641476D)
        goto fail;


    int sample_rate = read_32bitLE(0x08,streamFile);
    int channel_count = read_32bitLE(0x0c,streamFile);
    int blocks = read_32bitLE(0x10,streamFile);
    /* int unk = read_32bitLE(0x14,streamFile);*/ /* always 0 */
    int loop_start_block = read_32bitLE(0x18,streamFile);
    int loop_end_block = read_32bitLE(0x1c,streamFile);

    int loop_flag = loop_end_block > 0; /* loop_start_black can be 0 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* header data */
    vgmstream->coding_type = coding_PSX;
    vgmstream->meta_type = meta_PS2_SVAG_SNK;

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = blocks * 28;
    if( vgmstream->loop_flag ) {
        vgmstream->loop_start_sample = loop_start_block * 28;
        vgmstream->loop_end_sample = loop_end_block * 28;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;


    int start_offset = 0x20;
    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset =
                vgmstream->ch[i].offset =
                        start_offset + vgmstream->interleave_block_size*i;

        }
    }


    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
