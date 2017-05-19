#include "meta.h"
#include "../coding/coding.h"

#ifdef VGM_USE_FFMPEG

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

    /* init ffmpeg */
    ffmpeg_codec_data *data = init_ffmpeg_offset(streamFile, start, size);
    if (!data) return NULL;


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

#endif
