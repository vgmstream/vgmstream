#include "meta.h"
#include "../coding/coding.h"

/* TK5 (Tekken 5 Streams) */
VGMSTREAM * init_vgmstream_ps2_tk5(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("tk5",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x544B3553)
        goto fail;

    loop_flag = (read_32bitLE(0x0C,streamFile)!=0);
    channel_count = 2;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x800;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_PSX_badflags;
    vgmstream->num_samples = ((get_streamfile_size(streamFile)-0x800))/16*28/2;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_PS2_TK5;

    if (vgmstream->loop_flag)
    {
        vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile)/16*28;
        vgmstream->loop_end_sample = vgmstream->loop_start_sample  + (read_32bitLE(0x0C,streamFile)/16*28);
    }

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

/* OVB - Tekken 5 Streams from Tekken (NamCollection) */
VGMSTREAM * init_vgmstream_ps2_tk1(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "ovb"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x544B3553)
        goto fail;

    loop_flag = (read_32bitLE(0x0C,streamFile)!=0);
    channel_count = 2;
    start_offset = 0x800;
    /* NamCollection uses 44100 while Tekken 5 48000, no apparent way to tell them apart */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x08,streamFile)*channel_count, channel_count);
    if (vgmstream->loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile)/16*28;
        vgmstream->loop_end_sample = vgmstream->loop_start_sample  + ps_bytes_to_samples(read_32bitLE(0x0c,streamFile)*channel_count, channel_count);
    }

    vgmstream->coding_type = coding_PSX_badflags;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_PS2_TK1;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
