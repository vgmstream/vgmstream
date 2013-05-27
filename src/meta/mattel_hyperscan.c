#include "meta.h"
#include "../util.h"

/* Mattel Hyperscan,
- Place all metas for this console here (there are just 5 games) */
VGMSTREAM * init_vgmstream_hyperscan_kvag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bvg",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4B564147) /* "KVAG" */
        goto fail;

    channel_count = 1;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0xE;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x8,streamFile);
    vgmstream->coding_type = coding_DVI_IMA;
    vgmstream->num_samples = read_32bitLE(0x4,streamFile)*2;

#if 0
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitLE(0x08,streamFile)-1)*28;
        vgmstream->loop_end_sample = (read_32bitLE(0x0c,streamFile)-1)*28;
    }
#endif

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_HYPERSCAN_KVAG;

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
