#include "meta.h"
#include "../util.h"

/* SPS (from Ape Escape 2) */
VGMSTREAM * init_vgmstream_ps2_sps(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sps",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x10,streamFile) != 0x01000000)
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    
   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
   start_offset = 0x800;
   vgmstream->channels = channel_count;
   vgmstream->sample_rate = read_32bitLE(0x1C,streamFile);
   vgmstream->coding_type = coding_PCM16LE;
   vgmstream->num_samples = (read_32bitLE(0x18,streamFile)-0x800)/2/channel_count;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;
    vgmstream->meta_type = meta_PS2_SPS;

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
