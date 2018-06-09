#include "meta.h"
#include "../coding/coding.h"

#ifdef VGM_USE_FFMPEG

static int read_pos_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile);

/**
 * Generic init FFmpeg and vgmstream for any file supported by FFmpeg.
 * Called by vgmstream when trying to identify the file type (if the player allows it).
 */
VGMSTREAM * init_vgmstream_ffmpeg(STREAMFILE *streamFile) {
	return init_vgmstream_ffmpeg_offset( streamFile, 0, streamFile->get_size(streamFile) );
}

VGMSTREAM * init_vgmstream_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
    VGMSTREAM *vgmstream = NULL;
    int loop_flag = 0;
    int32_t loop_start = 0, loop_end = 0, num_samples = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;

    /* init ffmpeg */
    ffmpeg_codec_data *data = init_ffmpeg_offset(streamFile, start, size);
    if (!data) return NULL;

    total_subsongs = data->streamCount;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* try to get .pos data */
    {
        uint8_t posbuf[4+4+4];

        if ( read_pos_file(posbuf, 4+4+4, streamFile) ) {
            loop_start = get_32bitLE(posbuf+0);
            loop_end = get_32bitLE(posbuf+4);
            loop_flag = 1; /* incorrect looping will be validated outside */
            /* FFmpeg can't always determine totalSamples correctly so optionally load it (can be 0/NULL)
             * won't crash and will output silence if no loop points and bigger than actual stream's samples */
            num_samples = get_32bitLE(posbuf+8);
        }
    }


    /* build VGMSTREAM */
    vgmstream = allocate_vgmstream(data->channels, loop_flag);
    if (!vgmstream) goto fail;
    
    vgmstream->loop_flag = loop_flag;
    vgmstream->codec_data = data;
    vgmstream->channels = data->channels;
    vgmstream->sample_rate = data->sampleRate;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_FFmpeg;

    if (!num_samples) {
        num_samples = data->totalSamples;
    }
    vgmstream->num_samples = num_samples;

    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        vgmstream->loop_end_sample = loop_end;
    }

    /* this may happen for some streams if FFmpeg can't determine it */
    if (vgmstream->num_samples <= 0)
        goto fail;


    return vgmstream;
    
fail:
    free_ffmpeg(data);
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    
    return NULL;
}


/**
 * open file containing looping data and copy to buffer
 *
 * returns true if found and copied
 */
int read_pos_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile) {
    char posname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    /*size_t bytes_read;*/
    STREAMFILE * streamFilePos= NULL;

    streamFile->get_name(streamFile,filename,sizeof(filename));

    if (strlen(filename)+4 > sizeof(posname)) goto fail;

    /* try to open a posfile using variations: "(name.ext).pos" */
    {
        strcpy(posname, filename);
        strcat(posname, ".pos");
        streamFilePos = streamFile->open(streamFile,posname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFilePos) goto found;

        goto fail;
    }

found:
    //if (get_streamfile_size(streamFilePos) != bufsize) goto fail;

    /* allow pos files to be of different sizes in case of new features, just fill all we can */
    memset(buf, 0, bufsize);
    read_streamfile(buf, 0, bufsize, streamFilePos);

    close_streamfile(streamFilePos);

    return 1;

fail:
    if (streamFilePos) close_streamfile(streamFilePos);

    return 0;
}

#endif
