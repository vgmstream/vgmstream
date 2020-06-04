#include "meta.h"
#include "../coding/coding.h"

#ifdef VGM_USE_FFMPEG

static int read_pos_file(uint8_t* buf, size_t bufsize, STREAMFILE* sf);
static int find_ogg_loops(ffmpeg_codec_data* data, int32_t* p_loop_start, int32_t* p_loop_end);

/* parses any file supported by FFmpeg and not handled elsewhere (mainly: MP4/AAC, MP3, MPC, FLAC) */
VGMSTREAM* init_vgmstream_ffmpeg(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    ffmpeg_codec_data* data = NULL;
    int loop_flag = 0;
    int32_t loop_start = 0, loop_end = 0, num_samples = 0;
    int total_subsongs, target_subsong = sf->stream_index;

    /* no checks */
    //if (!check_extensions(sf, "..."))
    //    goto fail;

    /* don't try to open headers and other mini files */
    if (get_streamfile_size(sf) <= 0x1000)
        goto fail;


    /* init ffmpeg */
    data = init_ffmpeg_offset(sf, 0, get_streamfile_size(sf));
    if (!data) return NULL;

    total_subsongs = data->streamCount;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* try to get .pos data */
    {
        uint8_t posbuf[4+4+4];

        if (read_pos_file(posbuf, 4+4+4, sf)) {
            loop_start = get_s32le(posbuf+0);
            loop_end = get_s32le(posbuf+4);
            loop_flag = 1; /* incorrect looping will be validated outside */
            /* FFmpeg can't always determine totalSamples correctly so optionally load it (can be 0/NULL)
             * won't crash and will output silence if no loop points and bigger than actual stream's samples */
            num_samples = get_s32le(posbuf+8);
        }
    }

    /* try to read Ogg loop tags (abridged) */
    if (loop_flag == 0 && read_u32be(0x00, sf) == 0x4F676753) { /* "OggS" */
        loop_flag = find_ogg_loops(data, &loop_start, &loop_end);
    }

    /* hack for AAC files (will return 0 samples if not an actual file) */
    if (!num_samples && check_extensions(sf, "aac,laac")) {
        num_samples = aac_get_samples(sf, 0x00, get_streamfile_size(sf));
    }

#ifdef VGM_USE_MPEG
    /* hack for MP3 files (will return 0 samples if not an actual file) 
     *  .mus: Marc Ecko's Getting Up (PC) */
    if (!num_samples && check_extensions(sf, "mp3,lmp3,mus")) {
        num_samples = mpeg_get_samples(sf, 0x00, get_streamfile_size(sf));
    }
#endif

    /* hack for MPC, that seeks/resets incorrectly due to seek table shenanigans */
    if (read_u32be(0x00, sf) == 0x4D502B07 || /* "MP+\7" (Musepack V7) */
        read_u32be(0x00, sf) == 0x4D50434B) { /* "MPCK" (Musepack V8) */
        ffmpeg_set_force_seek(data);
    }

    /* default but often inaccurate when calculated using bitrate (wrong for VBR) */
    if (!num_samples) {
        num_samples = data->totalSamples; /* may be 0 if FFmpeg can't precalculate it */
    }


    /* build VGMSTREAM */
    vgmstream = allocate_vgmstream(data->channels, loop_flag);
    if (!vgmstream) goto fail;
    
    vgmstream->sample_rate = data->sampleRate;
    vgmstream->meta_type = meta_FFMPEG;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->codec_data = data;
    vgmstream->layout_type = layout_none;

    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->channel_layout = ffmpeg_get_channel_layout(vgmstream->codec_data);

    return vgmstream;
    
fail:
    free_ffmpeg(data);
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    
    return NULL;
}


/* open file containing looping data and copy to buffer, returns true if found and copied */
int read_pos_file(uint8_t* buf, size_t bufsize, STREAMFILE* sf) {
    char posname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    /*size_t bytes_read;*/
    STREAMFILE* sf_pos = NULL;

    get_streamfile_name(sf,filename,sizeof(filename));

    if (strlen(filename)+4 > sizeof(posname))
        goto fail;

    /* try to open a posfile using variations: "(name.ext).pos" */
    {
        strcpy(posname, filename);
        strcat(posname, ".pos");
        sf_pos = open_streamfile(sf, posname);;
        if (sf_pos) goto found;

        goto fail;
    }

found:
    //if (get_streamfile_size(sf_pos) != bufsize) goto fail;

    /* allow pos files to be of different sizes in case of new features, just fill all we can */
    memset(buf, 0, bufsize);
    read_streamfile(buf, 0, bufsize, sf_pos);

    close_streamfile(sf_pos);

    return 1;

fail:
    close_streamfile(sf_pos);
    return 0;
}


/* loop tag handling could be unified with ogg_vorbis.c, but that one has a extra features too */
static int find_ogg_loops(ffmpeg_codec_data* data, int32_t* p_loop_start, int32_t* p_loop_end) {
    char* endptr;
    const char* value;
    int loop_flag = 0;
    int32_t loop_start = -1, loop_end = -1;

    // Try to detect the loop flags based on current file metadata
    value = ffmpeg_get_metadata_value(data, "LoopStart");
    if (value != NULL) {
        loop_start = strtol(value, &endptr, 10);
        loop_flag = 1;
    }

    value = ffmpeg_get_metadata_value(data, "LoopEnd");
    if (value != NULL) {
        loop_end = strtol(value, &endptr, 10);
        loop_flag = 1;
    }

    if (loop_flag) {
        if (loop_end <= 0) {
            // Detected a loop, but loop_end is still undefined or wrong. Try to calculate it.
            value = ffmpeg_get_metadata_value(data, "LoopLength");
            if (value != NULL) {
                int loop_length = strtol(value, &endptr, 10);

                if (loop_start != -1) loop_end = loop_start + loop_length;
            }
        }

        if (loop_end <= 0) {
            // Looks a calculation was not possible, or tag value is wrongly set. Use the end of track as end value
            loop_end = data->totalSamples;
        }

        if (loop_start <= 0) {
            // Weird edge case: loopend is defined and there's a loop, but loopstart was never defined. Reset to sane value
            loop_start = 0;
        }
    } else {
        // Every other attempt to detect loop information failed, reset start/end flags to sane values
        loop_start = 0;
        loop_end = 0;
    }

    *p_loop_start = loop_start;
    *p_loop_end = loop_end;
    return loop_flag;
}

#endif
