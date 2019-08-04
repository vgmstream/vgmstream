#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG

/* init config and validate per type */
int mpeg_custom_setup_init_default(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type) {
    mpeg_frame_info info;


    /* get frame info at offset */
    if ( !mpeg_get_frame_info(streamFile, start_offset, &info))
        goto fail;
    switch(info.layer) {
        case 1: *coding_type = coding_MPEG_layer1; break;
        case 2: *coding_type = coding_MPEG_layer2; break;
        case 3: *coding_type = coding_MPEG_layer3; break;
        default: goto fail;
    }
    data->channels_per_frame = info.channels;
    data->samples_per_frame = info.frame_samples;
    data->bitrate_per_frame = info.bit_rate;
    data->sample_rate_per_frame = info.sample_rate;


    /* extra checks per type */
    switch(data->type) {
        case MPEG_XVAG:
            if (data->config.chunk_size <= 0 || data->config.interleave <= 0)
                goto fail; /* needs external fixed size */
            break;

        case MPEG_FSB:
            if (data->config.fsb_padding != 0
                    && data->config.fsb_padding != 2
                    && data->config.fsb_padding != 4
                    && data->config.fsb_padding != 16)
                goto fail; /* aligned to closest 0/2/4/16 bytes */

            /* get find interleave to stream offsets are set up externally */
            {
                int current_data_size = info.frame_size;
                int current_padding = 0;
                /* FSB padding for Layer III or multichannel Layer II */
                if ((info.layer == 3 && data->config.fsb_padding) || data->config.fsb_padding == 16) {
                    current_padding = (current_data_size % data->config.fsb_padding)
                            ? data->config.fsb_padding - (current_data_size % data->config.fsb_padding)
                            : 0;
                }

                data->config.interleave = current_data_size + current_padding; /* should be constant for all stream */
            }
            break;

        case MPEG_P3D:
        case MPEG_SCD:
            if (data->config.interleave <= 0)
                goto fail; /* needs external fixed size */
            break;

        case MPEG_LYN:
            if (data->config.interleave <= 0)
                goto fail; /* needs external fixed size */
            data->default_buffer_size = data->config.interleave;
            //todo simplify/unify XVAG/P3D/SCD/LYN and just feed arbitrary chunks to the decoder
            break;

        case MPEG_STANDARD:
        case MPEG_AHX:
        case MPEG_EA:
            if (info.channels != data->config.channels)
                goto fail; /* no multichannel expected */
            break;

        default:
            break;  /* nothing special needed */
    }


    //todo: test more: this improves the output, but seems formats aren't usually prepared
    // (and/or the num_samples includes all possible samples in file, so by discarding some it'll reach EOF)

    /* set encoder delay (samples to skip at the beginning of a stream) if needed, which varies with encoder used */
    switch(data->type) {
        //case MPEG_AHX: data->skip_samples = 480; break; /* observed default */
        //case MPEG_P3D: data->skip_samples = info.frame_samples; break; /* matches Radical ADPCM (PC) output */

        /* FSBs (with FMOD DLLs) don't seem to need it. Particularly a few games (all from Wayforward?)
         * contain audible garbage at the beginning, but it's actually there in-game too */
        //case MPEG_FSB: data->skip_samples = 0; break;

        case MPEG_XVAG: /* set in header and needed for gapless looping */
            data->skip_samples = data->config.skip_samples; break;
        case MPEG_STANDARD:
            data->skip_samples = data->config.skip_samples; break;
        default:
            break;
    }
    data->samples_to_discard = data->skip_samples;


    return 1;
fail:
    return 0;
}


/* writes data to the buffer and moves offsets */
int mpeg_custom_parse_frame_default(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    mpeg_frame_info info;
    size_t current_data_size = 0;
    size_t current_padding = 0;
    size_t current_interleave_pre = 0; /* interleaved data size before current stream */
    size_t current_interleave_post = 0; /* interleaved data size after current stream */
    size_t current_interleave = 0; /* interleave in this block (usually this + pre + post = interleave*streams = block) */


    /* Get data size to give to the decoder, per stream. Usually 1 frame at a time,
     * but doesn't really need to be a full frame (decoder would request more data). */
    switch(data->type) {

        case MPEG_XVAG: /* frames of fixed size (though we could read frame info too) */
            current_interleave = data->config.interleave; /* big interleave */
            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_size-1) - current_interleave_pre;

            current_data_size = data->config.chunk_size;
            break;

        case MPEG_FSB: /* frames with padding + interleave */
            current_interleave = data->config.interleave; /* constant for multi-stream FSbs (1 frame + padding) */
            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_size-1) - current_interleave_pre;

            if ( !mpeg_get_frame_info(stream->streamfile, stream->offset, &info) )
                goto fail;
            current_data_size = info.frame_size;

            /* get FSB padding for Layer III or multichannel Layer II (Layer I isn't supported by FMOD).
             * Padding sometimes contains garbage like the next frame header so we can't feed it to mpg123 or it gets confused. */
            if ((info.layer == 3 && data->config.fsb_padding) || data->config.fsb_padding == 16) {
                current_padding = (current_data_size % data->config.fsb_padding)
                        ? data->config.fsb_padding - (current_data_size % data->config.fsb_padding)
                        : 0;

                /* Rare Mafia II (PS3) bug (GP_0701_music multilang only): some frame paddings "4" are incorrect,
                 * calcs give 0xD0+0x00 but need 0xD0+0x04 (unlike all other fsbs, which never do that).
                 * FMOD tools decode fine, so they may be doing special detection too, since even
                 * re-encoding the same file and using the same FSB flags/modes won't trigger the bug. */
                if (info.layer == 3 && data->config.fsb_padding == 4 && current_data_size == 0xD0) {
                    uint32_t next_header;
                    off_t next_offset;

                    next_offset = stream->offset + current_data_size + current_padding;
                    if (current_interleave && ((next_offset - stream->channel_start_offset + current_interleave_pre + current_interleave_post) % current_interleave == 0)) {
                        next_offset += current_interleave_pre + current_interleave_post;
                    }

                    next_header = read_32bitBE(next_offset, stream->streamfile);
                    if ((next_header & 0xFFE00000) != 0xFFE00000) { /* doesn't land in a proper frame, fix sizes and hope */
                        VGM_LOG_ONCE("MPEG FSB: stream with wrong padding found\n");
                        current_padding = 0x04;
                    }
                }

            }

            VGM_ASSERT(data->streams_size > 1 && current_interleave != current_data_size+current_padding,
                    "MPEG FSB: %i streams with non-constant interleave found @ 0x%08x\n", data->streams_size, (uint32_t)stream->offset);
            break;

        case MPEG_P3D: /* fixed interleave, not frame-aligned (ie. blocks may end/start in part of a frame) */
        case MPEG_SCD:
        case MPEG_LYN:
            current_interleave = data->config.interleave;

            /* check if current interleave block is short */
            {
                off_t block_offset = stream->offset - stream->channel_start_offset;
                size_t next_block = data->streams_size*data->config.interleave;

                if (data->config.data_size && block_offset + next_block >= data->config.data_size)
                    current_interleave = (data->config.data_size % next_block) / data->streams_size; /* short_interleave*/
            }

            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_size-1) - current_interleave_pre;

            current_data_size = current_interleave;
            break;

        default: /* standard frames (CBR or VBR) */
            if ( !mpeg_get_frame_info(stream->streamfile, stream->offset, &info) )
                goto fail;
            current_data_size = info.frame_size;
            break;
    }
    if (!current_data_size || current_data_size > ms->buffer_size) {
        VGM_LOG("MPEG: incorrect data_size 0x%x vs buffer 0x%x\n", current_data_size, ms->buffer_size);
        goto fail;
    }

    /* This assumes all streams' offsets start in the first stream, and advances
     * the 'full interleaved block' at once, ex:
     *  start at s0=0x00, s1=0x00, interleave=0x40 (block = 0x40*2=0x80)
     *  @0x00 read 0x40 of s0, skip 0x40 of s1 (block of 0x80 done) > new offset = 0x80
     *  @0x00 skip 0x40 of s0, read 0x40 of s1 (block of 0x80 done) > new offset = 0x800
     */

    /* read chunk (skipping other interleaves if needed) */
    ms->bytes_in_buffer = read_streamfile(ms->buffer,stream->offset + current_interleave_pre, current_data_size, stream->streamfile);


    /* update offsets and skip other streams */
    stream->offset += current_data_size + current_padding;

    /* skip rest of block (interleave per stream) once this stream's interleaved data is done, if defined */
    if (current_interleave && ((stream->offset - stream->channel_start_offset + current_interleave_pre + current_interleave_post) % current_interleave == 0)) {
        stream->offset += current_interleave_pre + current_interleave_post;
    }


    return 1;
fail:
    return 0;
}


/*****************/
/* FRAME HELPERS */
/*****************/

/**
 * Gets info from a MPEG frame header at offset. Normally you would use mpg123_info but somehow
 * it's wrong at times (maybe because we use an ancient version) so here we do our thing.
 */
int mpeg_get_frame_info(STREAMFILE *streamfile, off_t offset, mpeg_frame_info * info) {
    /* index tables */
    static const int versions[4] = { /* MPEG 2.5 */ 3, /* reserved */ -1,  /* MPEG 2 */ 2, /* MPEG 1 */ 1 };
    static const int layers[4] = { -1,3,2,1 };
    static const int bit_rates[5][16] = { /* [version index ][bit rate index] (0=free, -1=bad) */
            { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1 }, /* MPEG1 Layer I */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1 }, /* MPEG1 Layer II */
            { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1 }, /* MPEG1 Layer III */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, -1 }, /* MPEG2/2.5 Layer I */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, -1 }, /* MPEG2/2.5 Layer II/III */
    };
    static const int sample_rates[4][4] = { /* [version][sample rate index] */
            { 44100, 48000, 32000, -1}, /* MPEG1 */
            { 22050, 24000, 16000, -1}, /* MPEG2 */
            { 11025, 12000,  8000, -1}, /* MPEG2.5 */
    };
    static const int channels[4] = { 2,2,2, 1 }; /* [channel] */
    static const int frame_samples[3][3] = { /* [version][layer] */
            { 384, 1152, 1152 }, /* MPEG1 */
            { 384, 1152, 576  }, /* MPEG2 */
            { 384, 1152, 576  }  /* MPEG2.5 */
    };

    uint32_t header;
    int idx, padding;


    memset(info, 0, sizeof(*info));

    header = read_32bitBE(offset, streamfile);

    if ((header >> 21) != 0x7FF) /* 31-21: sync */
        goto fail;

    info->version = versions[(header >> 19) & 0x3]; /* 20,19: version */
    if (info->version <= 0) goto fail;

    info->layer = layers[(header >> 17) & 0x3]; /* 18,17: layer */
    if (info->layer <= 0 || info->layer > 3) goto fail;

    //crc       = (header >> 16) & 0x1; /* 16: protected by crc? */

    idx = (info->version==1 ? info->layer-1 : (3 + (info->layer==1 ? 0 : 1)));
    info->bit_rate = bit_rates[idx][(header >> 12) & 0xf]; /* 15-12: bit rate */
    if (info->bit_rate <= 0) goto fail;

    info->sample_rate = sample_rates[info->version-1][(header >> 10) & 0x3]; /* 11-10: sampling rate */
    if (info->sample_rate <= 0) goto fail;

    padding     = (header >>  9) & 0x1; /* 9: padding? */
    //private   = (header >>  8) & 0x1; /* 8: private bit */

    info->channels = channels[(header >>  6) & 0x3]; /* 7,6: channel mode */

    //js_mode   = (header >>  4) & 0x3; /* 5,4: mode extension for joint stereo */
    //copyright = (header >>  3) & 0x1; /* 3: copyrighted */
    //original  = (header >>  2) & 0x1; /* 2: original */
    //emphasis  = (header >>  0) & 0x3; /* 1,0: emphasis */

    info->frame_samples = frame_samples[info->version-1][info->layer-1];

    /* calculate frame length (from hcs's fsb_mpeg) */
    switch (info->frame_samples) {
        case 384:  info->frame_size = (12l  * info->bit_rate * 1000l / info->sample_rate + padding) * 4; break; /* 384/32 = 12 */
        case 576:  info->frame_size = (72l  * info->bit_rate * 1000l / info->sample_rate + padding); break; /* 576/8 = 72 */
        case 1152: info->frame_size = (144l * info->bit_rate * 1000l / info->sample_rate + padding); break; /* 1152/8 = 144 */
        default: goto fail;
    }

    return 1;

fail:
    return 0;
}

size_t mpeg_get_samples(STREAMFILE *streamFile, off_t start_offset, size_t bytes) {
    off_t offset = start_offset;
    off_t max_offset = start_offset + bytes;
    int samples = 0;
    mpeg_frame_info info;
    size_t prev_size = 0;
    int cbr_count = 0;
    int is_vbr = 0;

    if (!streamFile)
        return 0;

    if (max_offset > get_streamfile_size(streamFile))
        max_offset = get_streamfile_size(streamFile);

    /* MPEG may use VBR so must read all frames */
    while (offset < max_offset) {

        /* skip ID3v2 */
        if ((read_32bitBE(offset+0x00, streamFile) & 0xFFFFFF00) == 0x49443300) { /* "ID3\0" */
            size_t frame_size = 0;
            uint8_t flags = read_8bit(offset+0x05, streamFile);
            /* this is how it's officially read :/ */
            frame_size += read_8bit(offset+0x06, streamFile) << 21;
            frame_size += read_8bit(offset+0x07, streamFile) << 14;
            frame_size += read_8bit(offset+0x08, streamFile) << 7;
            frame_size += read_8bit(offset+0x09, streamFile) << 0;
            frame_size += 0x0a;
            if (flags & 0x10) /* footer? */
                frame_size += 0x0a;

            offset += frame_size;
            continue;
        }

        /* this may fail with unknown ID3 tags */
        if (!mpeg_get_frame_info(streamFile, offset, &info))
            break;

        if (prev_size && prev_size != info.frame_size) {
            is_vbr = 1;
        }
        else if (!is_vbr) {
            cbr_count++;
        }

        if (cbr_count >= 10) {
            /* must be CBR, don't bother counting */
            samples = (bytes / info.frame_size) * info.frame_samples;
            break;
        }

        offset += info.frame_size;
        prev_size = info.frame_size;
        samples += info.frame_samples; /* header frames may be 0? */
    }

    return samples;
}

#endif
