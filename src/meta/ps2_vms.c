#include "meta.h"
#include "../util.h"

/* VMS (Autobahn Raser: Police Madness [SLES-53536]) */
VGMSTREAM * init_vgmstream_ps2_vms(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    
    int loop_flag = 0;
    int channel_count;
    int header_size;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("vms",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x564D5320)
        goto fail;

    loop_flag = 1;
    channel_count = read_8bit(0x08,streamFile);
    header_size = read_32bitLE(0x1C, streamFile);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = header_size;

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x14,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = ((get_streamfile_size(streamFile) - header_size)/16/ channel_count * 28);

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x10,streamFile);
    vgmstream->meta_type = meta_PS2_VMS;
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = (get_streamfile_size(streamFile))/16/ channel_count * 28;

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
