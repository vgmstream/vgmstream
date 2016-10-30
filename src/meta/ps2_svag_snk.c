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

    off_t start_offset = 0x20;

    int loop_flag;
    int channel_count;
    int loop_start_block;
    int loop_end_block;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("svag",filename_extension(filename))) goto fail;

    /* check SNK SVAG Header ("VAGm") */
    if (read_32bitBE(0x00,streamFile) != 0x5641476D)
        goto fail;


    channel_count = read_32bitLE(0x0c,streamFile);

    loop_start_block = read_32bitLE(0x18,streamFile);
    loop_end_block = read_32bitLE(0x1c,streamFile);

    loop_flag = loop_end_block > 0; /* loop_start_block can be 0 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* header data */
    vgmstream->coding_type = coding_PSX;
    vgmstream->meta_type = meta_PS2_SVAG_SNK;

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = read_32bitLE(0x10,streamFile) * 28; /* size in blocks */
    if( vgmstream->loop_flag ) {
        vgmstream->loop_start_sample = loop_start_block * 28;
        vgmstream->loop_end_sample = loop_end_block * 28;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;


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
