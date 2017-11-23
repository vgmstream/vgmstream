#include "meta.h"
#include "../coding/coding.h"

/* SMPL - from Homura */
VGMSTREAM * init_vgmstream_ps2_smpl(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamRch = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t channel_size;

    /* check extension (.v0: left channel, .v1: right channel, .smpl: header id) */
    if ( !check_extensions(streamFile,"v0,smpl") )
       goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x534D504C) /* "SMPL" */
        goto fail;

    /* right channel is in .V1 and doesn't have loop points; manually parse as dual_stereo would fail */
    streamRch = open_stream_ext(streamFile, "V1");
    channel_count = streamRch != NULL ? 2 : 1;
    loop_flag = (read_32bitLE(0x30,streamFile) != 0);
    start_offset = 0x40;
    channel_size = read_32bitBE(0x0c,streamFile) - 0x10;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size*channel_count, channel_count);
    vgmstream->loop_start_sample = read_32bitLE(0x30,streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_PS2_SMPL;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;

    /* always, but can be null or used as special string */
    read_string(vgmstream->stream_name,0x10+1, 0x20,streamFile);

    /* open the file for reading */
    //if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
    //    goto fail;

    /* custom dual channel */ // todo improve dual_stereo
    {
        int i;
        char filename[PATH_LIMIT];

        streamFile->get_name(streamFile,filename,sizeof(filename));
        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!vgmstream->ch[0].streamfile) goto fail;

        if (channel_count == 2)
            vgmstream->ch[1].streamfile = streamRch;

        for (i = 0; i < channel_count; i++) {
            vgmstream->ch[i].channel_start_offset =
                vgmstream->ch[i].offset = start_offset;
        }
    }

    return vgmstream;

fail:
    if (streamRch) close_streamfile(streamRch);
    close_vgmstream(vgmstream);
    return NULL;
}
