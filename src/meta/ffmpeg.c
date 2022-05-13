#include "meta.h"
#include "../coding/coding.h"

#ifdef VGM_USE_FFMPEG

static int read_pos_file(uint8_t* buf, size_t bufsize, STREAMFILE* sf);
static int find_meta_loops(ffmpeg_codec_data* data, int32_t* p_loop_start, int32_t* p_loop_end);

/* parses any format supported by FFmpeg and not handled elsewhere:
 * - MPC (.mpc, mp+): Moonshine Runners (PC), Asphalt 7 (PC)
 * - FLAC (.flac):  Warcraft 3 Reforged (PC), Call of Duty: Ghosts (PC)
 * - DUCK (.wav): Sonic Jam (SAT), Virtua Fighter 2 (SAT)
 * - ALAC/AAC (.caf): Chrono Trigger (iOS)
 * - ATRAC3 (.oma, .aa3): demuxed PSP/PS3 videos
 * - WMA/WMAPRO (.wma): Castlevania Symphony of the Night (Xbox)
 * - AC3 (.ac3): some PS2 games
 *
 * May also catch files that are supported elsewhere but rejected due to bugs,
 * but those should be fixed in their parser for proper loops/etc support
 * (catch-all behavior may be disabled later).
  */
VGMSTREAM* init_vgmstream_ffmpeg(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    ffmpeg_codec_data* data = NULL;
    int loop_flag = 0, channels, sample_rate;
    int32_t loop_start = 0, loop_end = 0, num_samples = 0, encoder_delay = 0;
    int total_subsongs, target_subsong = sf->stream_index;
    int faulty = 0; /* mark wonky rips in hopes people may fix them */

    /* no checks */
    //if (!check_extensions(sf, "..."))
    //    goto fail;

    /* don't try to open headers and other mini files */
    if (get_streamfile_size(sf) <= 0x1000)
        goto fail;

    // many PSP rips have poorly demuxed videos with a failty RIFF, allow for now
#if 0
    /* reject some formats handled elsewhere (better fail and check there than let buggy FFmpeg take over) */
    if (check_extensions(sf, "at3"))
        goto fail;
#endif

    if (target_subsong == 0) target_subsong = 1;

    /* init ffmpeg */
    data = init_ffmpeg_header_offset_subsong(sf, NULL, 0, 0, get_streamfile_size(sf), target_subsong);
    if (!data) return NULL;

    total_subsongs = ffmpeg_get_subsong_count(data); /* uncommon, ex. wmv [Lost Odyssey (X360)] */
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* try to get .pos data */
    {
        uint8_t posbuf[0x04*3];

        if (read_pos_file(posbuf, sizeof(posbuf), sf)) {
            loop_start = get_s32le(posbuf+0x00);
            loop_end = get_s32le(posbuf+0x04);
            loop_flag = 1; /* incorrect looping will be validated outside */
            /* FFmpeg can't always determine samples correctly so optionally load it (can be 0/NULL)
             * won't crash and will output silence if no loop points and bigger than actual stream's samples */
            num_samples = get_s32le(posbuf+8);
        }
    }

    /* try to read Ogg/Flac loop tags (abridged) */
    if (!loop_flag && (is_id32be(0x00, sf, "OggS") || is_id32be(0x00, sf, "fLaC"))) {
        loop_flag = find_meta_loops(data, &loop_start, &loop_end);
    }

    /* hack for AAC files (will return 0 samples if not an actual file) */
    if (!num_samples && check_extensions(sf, "aac,laac")) {
        num_samples = aac_get_samples(sf, 0x00, get_streamfile_size(sf));

        if (num_samples > 0) {
            /* FFmpeg seeks to 0 eats first frame for whatever reason */
            ffmpeg_set_force_seek(data);
        }
    }

    /* hack for MP3 files (will return 0 samples if not an actual file) 
     *  .mus: Marc Ecko's Getting Up (PC) */
    if (!num_samples && check_extensions(sf, "mp3,lmp3,mus")) {
        num_samples = mpeg_get_samples(sf, 0x00, get_streamfile_size(sf));

        /* this seems correct thankfully */
        //ffmpeg_set_skip_samples(data, encoder_delay);
    }

    /* hack for MPC */
    if (is_id32be(0x00, sf, "MP+\x07") ||   /* Musepack V7 */
        is_id32be(0x00, sf, "MP+\x17") ||   /* Musepack V7 with flag (seen in FFmpeg) */
        is_id32be(0x00, sf, "MPCK")) {      /* Musepack V8 */
        /* FFmpeg seeks/resets incorrectly due to MPC seek table shenanigans */
        ffmpeg_set_force_seek(data);

        /* FFmpeg gets this wrong as usual (specially V8 samples) */
        mpc_get_samples(sf, 0x00, &num_samples, &encoder_delay);

        ffmpeg_set_skip_samples(data, encoder_delay);
    }

    /* detect broken RIFFs */
    if (is_id32be(0x00, sf, "RIFF")) {
        uint32_t size = read_u32le(0x04, sf);
        /* There is a log in RIFF too but to be extra sure and sometimes FFmpeg don't handle it (this is mainly for wrong AT3).
         * Some proper RIFF can be parsed here too (like DUCK). */
        if (size + 0x08 > get_streamfile_size(sf)) {
            vgm_logi("RIFF/FFmpeg: incorrect size, file may have missing data\n");
            faulty = 1;
        }
        else if (size + 0x08 < get_streamfile_size(sf)) {
            vgm_logi("RIFF/FFmpeg: incorrect size, file may have padded data\n");
            faulty = 1;
        }
    }

    /* default but often inaccurate when calculated using bitrate (wrong for VBR) */
    if (!num_samples) {
        num_samples = ffmpeg_get_samples(data); /* may be 0 if FFmpeg can't precalculate it */
    }

    channels = ffmpeg_get_channels(data);
    sample_rate = ffmpeg_get_sample_rate(data);


    /* build VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = faulty ? meta_FFMPEG_faulty : meta_FFMPEG;
    vgmstream->sample_rate = sample_rate;

    vgmstream->codec_data = data;
    vgmstream->coding_type = coding_FFmpeg;
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


/* loop tag handling could be unified with ogg_vorbis.c, but that one has a extra features too.
 * Also has support for flac meta loops, that can be used by stuff like Platformer Game Engine
 * or ZDoom/Duke Nukem 3D source ports (maybe RPG Maker too). */
//todo call ffmpeg_get_next_tag and iterate like ogg_vorbis.c
//todo also save title
static int find_meta_loops(ffmpeg_codec_data* data, int32_t* p_loop_start, int32_t* p_loop_end) {
    char* endptr;
    const char* value;
    int loop_flag = 0;
    int32_t loop_start = -1, loop_end = -1;

    // Try to detect the loop flags based on current file metadata (ignores case)
    value = ffmpeg_get_metadata_value(data, "LoopStart");
    if (!value)
        value = ffmpeg_get_metadata_value(data, "LOOP_START"); /* ZDoom/DN3D */
    if (value) {
        loop_start = strtol(value, &endptr, 10);
        loop_flag = 1;
    }

    value = ffmpeg_get_metadata_value(data, "LoopEnd");
    if (!value)
        value = ffmpeg_get_metadata_value(data, "LOOP_END"); /* ZDoom/DN3D */
    if (value) {
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
            loop_end = ffmpeg_get_samples(data);
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
