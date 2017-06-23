#include "meta.h"
#include "../coding/coding.h"

/* ULW - headerless U-law, found in Burnout (GC) */
VGMSTREAM * init_vgmstream_ngc_ulw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;


    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"ulw"))
        goto fail;

    /* raw data, the info is in the filename (really!) */
    {
        char* path;
        char basename[PATH_LIMIT];
        char filename[PATH_LIMIT];

        /* get base name */
        streamFile->get_name(streamFile,filename,sizeof(filename));
        path = strrchr(filename,DIR_SEPARATOR);
        if (path!=NULL)
            path = path+1;
        else
            path = filename;
        strcpy(basename,path);

        /* first letter gives the channels */
        if (basename[0]=='M') /* Mono */
            channel_count = 1; 
        else if (basename[0]=='S' || basename[0]=='D') /* Stereo/Dolby */
            channel_count = 2;
        else
            goto fail;

        /* not very robust but meh (other tracks don't loop) */
        if (strcmp(basename,"MMenu.ulw")==0 || strcmp(basename,"DMenu.ulw")==0) {
            loop_flag = 1;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 32000;
    vgmstream->coding_type = coding_ULAW;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->meta_type = meta_NGC_ULW;
    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(streamFile), channel_count, 8);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    start_offset = 0;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
