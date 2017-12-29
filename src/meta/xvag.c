#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

static int ps_adpcm_find_loop_offsets(STREAMFILE *streamFile, int channel_count, off_t start_offset, off_t * loop_start, off_t * loop_end);

/* XVAG - Sony's Scream Tool/Stream Creator format (God of War III, Ratchet & Clank Future, The Last of Us, Uncharted) */
VGMSTREAM * init_vgmstream_xvag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int loop_flag = 0, channel_count, codec;
    int big_endian;
    int sample_rate, num_samples, multiplier;
    int total_subsongs = 0, target_subsong = streamFile->stream_index;

    off_t start_offset, loop_start, loop_end, chunk_offset;
    off_t first_offset = 0x20;
    size_t chunk_size;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"xvag")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58564147) /* "XVAG" */
        goto fail;

    /* empty start_offset > little endian (also, XVAGs of the same game can use BE or LE, oddly enough) */
    big_endian = read_8bit(0x07,streamFile) != 0;
    if (big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    start_offset = read_32bit(0x04,streamFile);
    /* 0x08: flags? (&0x01=big endian?)  0x0a: version (chunk sizes vary) */

    /* "fmat": base format */
    if (!find_chunk(streamFile, 0x666D6174,first_offset,0, &chunk_offset,&chunk_size, big_endian, 1)) /*"fmat"*/
        goto fail;
    channel_count = read_32bit(chunk_offset+0x00,streamFile);
    codec = read_32bit(chunk_offset+0x04,streamFile);
    num_samples = read_32bit(chunk_offset+0x08,streamFile);
    /* 0x0c: samples again? */
    multiplier = read_32bit(chunk_offset+0x10,streamFile);
    sample_rate = read_32bit(chunk_offset+0x14,streamFile);
    /* 0x18: datasize */
    if (chunk_size > 0x1c) {
        total_subsongs = read_32bit(chunk_offset+0x1c,streamFile); /* number of interleaved subsongs */
        /* 0x20: number of multichannel substreams (for MPEG) */

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    }


    /* other chunks: */
    /* "cpan": pan/volume per channel */
    /* "0000": end chunk before start_offset */

    /* some XVAG seem to do full loops, this should detect them as looping */
    if (codec == 0x06) {
        loop_flag = ps_adpcm_find_loop_offsets(streamFile, channel_count, start_offset, &loop_start, &loop_end);
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->meta_type = meta_XVAG;

    switch (codec) {
        case 0x06:   /* PS ADPCM: God of War III, Uncharted 1/2, Ratchet and Clank Future */
        case 0x07: { /* Bizarro 6ch PS ADPCM: infamous 1 (todo won't play properly; algo tweak + bigger predictor table?) */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;//* multiplier? (doesn't seem necessary, always 1);
            vgmstream->coding_type = coding_PSX;

            if (loop_flag) {
                vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, vgmstream->channels);
                vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, vgmstream->channels);
            }
            break;
        }

#ifdef VGM_USE_MPEG
        case 0x08: { /* MPEG: The Last of Us, Uncharted 3, Medieval Moves */
            mpeg_custom_config cfg = {0};

            /* "mpin": mpeg info */
            /*  0x00/04: mpeg version/layer?  other: unknown or repeats of "fmat" */
            if (!find_chunk(streamFile, 0x6D70696E,first_offset,0, &chunk_offset,NULL, big_endian, 1)) /*"mpin"*/
                goto fail;

            cfg.chunk_size = read_32bit(chunk_offset+0x1c,streamFile); /* fixed frame size */
            cfg.interleave = cfg.chunk_size * multiplier;

            vgmstream->codec_data = init_mpeg_custom_codec_data(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_XVAG, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x09: { /* ATRAC9: Sly Cooper and the Thievius Raccoonus, The Last of Us Remastered */
            atrac9_config cfg = {0};

            /* "a9in": ATRAC9 info */
            /*  0x00: block align, 0x04: samples per block, 0x0c: fact num_samples (no change), 0x10: encoder delay1 */
            if (!find_chunk(streamFile, 0x6139696E,first_offset,0, &chunk_offset,NULL, big_endian, 1)) /*"a9in"*/
                goto fail;

            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bitBE(chunk_offset+0x08,streamFile);
            cfg.encoder_delay = read_32bit(chunk_offset+0x14,streamFile);

            /* Sly Cooper interleaves 'multiplier' superframes per subsong (all share config_data) */
            cfg.interleave_skip = read_32bit(chunk_offset+0x00,streamFile) * multiplier;
            cfg.subsong_skip = total_subsongs;
            start_offset += (target_subsong-1) * cfg.interleave_skip * (cfg.subsong_skip-1);

            /* The Last of Us Remastered has an AT9 RIFF header inside (same values, can be ignored) */
            if (read_32bitBE(start_offset+0x00, streamFile) == 0x00000000 &&
                read_32bitBE(start_offset+0x9c, streamFile) == 0x52494646) { /*"RIFF"*/
                if (!find_chunk(streamFile, 0x64617461,start_offset+0x9c+0x0c,0, &start_offset,NULL, big_endian, 0)) /*"data"*/
                    goto fail;
            }

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


static int ps_adpcm_find_loop_offsets(STREAMFILE *streamFile, int channel_count, off_t start_offset, off_t * loop_start, off_t * loop_end) {
    uint8_t testBuffer[0x10];
    int     loopStartPointsCount=0;
    int     loopEndPointsCount=0;
    off_t   readOffset = 0;
    off_t   loopStartPoints[0x10];
    off_t   loopEndPoints[0x10];

    off_t   loopStart = 0;
    off_t   loopEnd = 0;
    off_t fileLength;
    int loop_flag = 0;

    readOffset=start_offset;
    fileLength = get_streamfile_size(streamFile);

    // get the loops the same way we get on .MIB
    do {
        readOffset+=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile);

        // Loop Start ...
        if(testBuffer[0x01]==0x06) {
            if(loopStartPointsCount<0x10) {
                loopStartPoints[loopStartPointsCount] = readOffset-0x10;
                loopStartPointsCount++;
            }
        }

        // Loop End ...
        if(((testBuffer[0x01]==0x03) && (testBuffer[0x03]!=0x77)) || (testBuffer[0x01]==0x01)) {
            if(loopEndPointsCount<0x10) {
                loopEndPoints[loopEndPointsCount] = readOffset; //-0x10;
                loopEndPointsCount++;
            }
        }

    } while (readOffset<((int32_t)fileLength));

    // Calc Loop Points & Interleave ...
    if(loopStartPointsCount>=channel_count) {
        // can't get more then 0x10 loop point !
        if((loopStartPointsCount<=0x0F) && (loopStartPointsCount>=2)) {
            // Always took the first 2 loop points
            loopStart=loopStartPoints[1]-start_offset;
            loop_flag=1;
        } else {
            loopStart=0;
        }
    }

    if(loopEndPointsCount>=channel_count) {
        // can't get more then 0x10 loop point !
        if((loopEndPointsCount<=0x0F) && (loopEndPointsCount>=2)) {
            loop_flag=1;
            loopEnd=loopEndPoints[loopEndPointsCount-1]-start_offset;
        } else {
            loopEnd=0;
        }
    }

    // as i can get on the header if a song is looped or not
    // if try to take the loop marker on the file
    // if the last byte of the file is = 00 is assume that the song is not looped
    // i know that i can cover 95% of the file, but can't work on some of them
    if(read_8bit((fileLength-1),streamFile)==0)
        loop_flag=0;

    if (loop_flag) {
        *loop_start = loopStart;
        *loop_end = loopEnd;
    }

    return loop_flag;
}
