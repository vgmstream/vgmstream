#include "coding.h"
#include "../util.h"
#include "../vgmstream.h"

#ifdef VGM_USE_MPEG
#include <mpg123.h>
#include "mpeg_decoder.h"


/* TODO list for custom decoder
 * - don't force channel param and get them from frame headers for some types (for MPEG_STANDARD)
 * - in case of streams like 2ch+1ch+1ch+2ch (not seen) use one stream per channel and advance streams as channels are done
 * - validate of channels between streams
 */

#define MPEG_DATA_BUFFER_SIZE 0x1000 /* at least one MPEG frame (max ~0x5A1 plus some more in case of free bitrate) */

static mpg123_handle * init_mpg123_handle();
static void decode_mpeg_standard(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
static void decode_mpeg_custom(VGMSTREAM * vgmstream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
static void decode_mpeg_custom_stream(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, int num_stream);


/* Inits regular MPEG */
mpeg_codec_data *init_mpeg_codec_data(STREAMFILE *streamfile, off_t start_offset, coding_t *coding_type, int channels) {
    mpeg_codec_data *data = NULL;

    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DATA_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;

    data->m = init_mpg123_handle();
    if (!data->m) goto fail;

    /* check format */
    {
        mpg123_handle *main_m = data->m;
        off_t read_offset = 0;
        int rc;

        long sample_rate_per_frame;
        int channels_per_frame, encoding;
        size_t samples_per_frame;
        struct mpg123_frameinfo mi;

        /* read first frame(s) */
        do {
            size_t bytes_done;
            if (read_streamfile(data->buffer, start_offset+read_offset, data->buffer_size, streamfile) != data->buffer_size)
                goto fail;
            read_offset+=1;

            rc = mpg123_decode(main_m, data->buffer,data->buffer_size, NULL,0, &bytes_done);
            if (rc != MPG123_OK && rc != MPG123_NEW_FORMAT && rc != MPG123_NEED_MORE) {
                VGM_LOG("MPEG: unable to set up mpg123 @ 0x%08lx to 0x%08lx\n", start_offset, read_offset);
                goto fail; //handle MPG123_DONE?
            }
            if (read_offset > 0x5000) { /* don't hang in some incorrectly detected formats */
                VGM_LOG("MPEG: unable to find mpeg data @ 0x%08lx to 0x%08lx\n", start_offset, read_offset);
                goto fail;
            }

        } while (rc != MPG123_NEW_FORMAT);

        /* check first frame header and validate */
        rc = mpg123_getformat(main_m,&sample_rate_per_frame,&channels_per_frame,&encoding);
        if (rc != MPG123_OK) goto fail;

        mpg123_info(main_m,&mi);

        if (encoding != MPG123_ENC_SIGNED_16)
            goto fail;
        if (sample_rate_per_frame != mi.rate)
            goto fail;
        if ((channels != -1 && channels_per_frame != channels))
            goto fail;

        switch(mi.layer) {
            case 1: *coding_type = coding_MPEG_layer1; break;
            case 2: *coding_type = coding_MPEG_layer2; break;
            case 3: *coding_type = coding_MPEG_layer3; break;
            default: goto fail;
        }

        if (mi.layer == 1)
            samples_per_frame = 384;
        else if (mi.layer == 2)
            samples_per_frame = 1152;
        else if (mi.layer == 3 && mi.version == MPG123_1_0) //MP3
            samples_per_frame = 1152;
        else if (mi.layer == 3)
            samples_per_frame = 576;
        else goto fail;

        data->channels_per_frame = channels_per_frame;
        data->samples_per_frame = samples_per_frame;
        if (channels_per_frame != channels)
            goto fail;


        /* reinit, to ignore the reading done */
        mpg123_open_feed(main_m);
    }

    return data;

fail:
    free_mpeg(data);
    return NULL;
}


/* Init custom MPEG, with given type and config */
mpeg_codec_data *init_mpeg_custom_codec_data(STREAMFILE *streamFile, off_t start_offset, coding_t *coding_type, int channels, mpeg_custom_t type, mpeg_custom_config *config) {
    mpeg_codec_data *data = NULL;
    int i, ok;

    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DATA_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;


    /* keep around to decode */
    data->custom = 1;
    data->type = type;
    memcpy(&data->config, config, sizeof(mpeg_custom_config));
    data->config.channels = channels;

    /* init per subtype */
    switch(data->type) {
        case MPEG_EAL31:
        case MPEG_EAL31b:
        case MPEG_EAL32P:
        case MPEG_EAL32S:   ok = mpeg_custom_setup_init_ealayer3(streamFile, start_offset, data, coding_type); break;
        case MPEG_AWC:      ok = mpeg_custom_setup_init_awc(streamFile, start_offset, data, coding_type); break;
        default:            ok = mpeg_custom_setup_init_default(streamFile, start_offset, data, coding_type); break;
    }
    if (!ok)
        goto fail;

    if (channels <= 0 || channels > 16) goto fail; /* arbitrary max */
    if (channels < data->channels_per_frame) goto fail;


    /* init streams */
    data->streams_size = channels / data->channels_per_frame;
    data->streams = calloc(data->streams_size, sizeof(mpeg_custom_stream*));
    for (i=0; i < data->streams_size; i++) {
        data->streams[i] = calloc(1, sizeof(mpeg_custom_stream));
        data->streams[i]->m = init_mpg123_handle(); /* decoder not shared as may need several frames to decode)*/
        if (!data->streams[i]->m) goto fail;

        /* size could be any value */
        data->streams[i]->output_buffer_size = sizeof(sample) * data->channels_per_frame * data->samples_per_frame;
        data->streams[i]->output_buffer = calloc(data->streams[i]->output_buffer_size, sizeof(uint8_t));
        if (!data->streams[i]->output_buffer) goto fail;
    }


    /* write output */
    config->interleave = data->config.interleave; /* for FSB */

    return data;

fail:
    free_mpeg(data);
    return NULL;
}


static mpg123_handle * init_mpg123_handle() {
    mpg123_handle *m = NULL;
    int rc;

    /* inits a new mpg123 handle */
    m = mpg123_new(NULL,&rc);
    if (rc == MPG123_NOT_INITIALIZED) {
        /* inits the library if needed */
        if (mpg123_init() != MPG123_OK)
            goto fail;
        m = mpg123_new(NULL,&rc);
        if (rc != MPG123_OK) goto fail;
    } else if (rc != MPG123_OK) {
        goto fail;
    }

    mpg123_param(m,MPG123_REMOVE_FLAGS,MPG123_GAPLESS,0.0); /* wonky support */
    mpg123_param(m,MPG123_RESYNC_LIMIT, -1, 0x10000); /* should be enough */

    if (mpg123_open_feed(m) != MPG123_OK) {
        goto fail;
    }

    return m;

fail:
    mpg123_delete(m);
    return NULL;
}


/************/
/* DECODERS */
/************/

void decode_mpeg(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels) {
    mpeg_codec_data * data = (mpeg_codec_data *) vgmstream->codec_data;

    if (!data->custom) {
        decode_mpeg_standard(&vgmstream->ch[0], data, outbuf, samples_to_do, channels);
    } else {
        decode_mpeg_custom(vgmstream, data, outbuf, samples_to_do, channels);
    }
}

/**
 * Decode anything mpg123 can.
 * Feeds raw data and extracts decoded samples as needed.
 */
static void decode_mpeg_standard(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;
    mpg123_handle *m = data->m;

    while (samples_done < samples_to_do) {
        size_t bytes_done;
        int rc;

        /* read more raw data */
        if (!data->buffer_full) {
            data->bytes_in_buffer = read_streamfile(data->buffer,stream->offset,data->buffer_size,stream->streamfile);

            /* end of stream, fill rest with 0s */
			if (!data->bytes_in_buffer) {
				memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * channels * sizeof(sample));
				break;
			}

            data->buffer_full = 1;
            data->buffer_used = 0;

            stream->offset += data->bytes_in_buffer;
        }

        /* feed new raw data to the decoder if needed, copy decoded results to output */
        if (!data->buffer_used) {
            rc = mpg123_decode(m,
                    data->buffer,data->bytes_in_buffer,
                    (unsigned char *)(outbuf+samples_done*channels),
                    (samples_to_do-samples_done)*sizeof(sample)*channels,
                    &bytes_done);
            data->buffer_used = 1;
        }
        else {
            rc = mpg123_decode(m,
                    NULL,0,
                    (unsigned char *)(outbuf+samples_done*channels),
                    (samples_to_do-samples_done)*sizeof(sample)*channels,
                    &bytes_done);
        }

        /* not enough raw data, request more */
        if (rc == MPG123_NEED_MORE) {
            data->buffer_full = 0;
        }

        /* update copied samples */
        samples_done += bytes_done/sizeof(sample)/channels;
    }
}


/**
 * Decode custom MPEG, for: single frames, mutant frames, interleave/multiple streams (Nch = 2ch*N/2 or 1ch*N), etc.
 *
 * Copies to outbuf when there are samples in all streams and calls decode_mpeg_custom_stream to decode.
 . Depletes the stream's sample buffers before decoding more, so it doesn't run out of buffer space.
 */
static void decode_mpeg_custom(VGMSTREAM * vgmstream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
    int i, samples_done = 0;

    while (samples_done < samples_to_do) {
        int samples_to_copy = -1;

        /* find max to copy from all streams (equal for all channels) */
        for (i = 0; i < data->streams_size; i++) {
            size_t samples_in_stream = data->streams[i]->samples_filled -  data->streams[i]->samples_used;
            if (samples_to_copy < 0 || samples_in_stream < samples_to_copy)
                samples_to_copy = samples_in_stream;
        }


        /* discard if needed (for looping) */
        if (data->samples_to_discard) {
            int samples_to_discard = samples_to_copy;
            if (samples_to_discard > data->samples_to_discard)
                samples_to_discard = data->samples_to_discard;

            for (i = 0; i < data->streams_size; i++) {
                data->streams[i]->samples_used += samples_to_discard;
            }
            data->samples_to_discard -= samples_to_discard;
            samples_to_copy -= samples_to_discard;
        }


        if (samples_to_copy > 0) {
            /* copy stream's samples to outbuf */
            if (samples_to_copy > samples_to_do - samples_done)
                samples_to_copy = samples_to_do - samples_done;

            /* mux streams channels (1/2ch) to outbuf (Nch) (ex. 6ch: samples from 2ch+2ch+2ch) */
            for (i = 0; i < data->streams_size; i++) {
                mpeg_custom_stream *ms = data->streams[i];
                int channels_frame = data->channels_per_frame;
                int fch, s;

                for (fch = 0; fch < channels_frame; fch++) {
                    for (s = 0; s < samples_to_copy; s++) {
                        size_t bytes_used = sizeof(sample)*ms->samples_used*channels_frame;
                        off_t in_offset  = sizeof(sample)*s*channels_frame + sizeof(sample)*fch;
                        off_t out_offset = s*channels + i*channels_frame + fch;

                        memcpy((uint8_t*)(outbuf+samples_done*channels + out_offset),
                               ms->output_buffer+bytes_used + in_offset,
                               sizeof(sample));
                    }
                }

                ms->samples_used += samples_to_copy;
            }

            samples_done += samples_to_copy;
        }
        else {
            /* decode more into stream sample buffers */

            /* Handle offsets depending on the data layout (may only use half VGMSTREAMCHANNELs with 2ch streams)
             * With multiple offsets they should already start in the first frame of each stream. */
            for (i=0; i < data->streams_size; i++) {
                switch(data->type) {
                  //case MPEG_FSB:
                        /* same offset: alternate frames between streams (maybe needed for weird layouts?) */
                        //decode_mpeg_custom_stream(&vgmstream->ch[0], data, i);

                    default:
                        /* offset per stream: absolute offsets, fixed interleave (skips other streams/interleave) */
                        decode_mpeg_custom_stream(&vgmstream->ch[i], data, i);
                        break;
                }
            }
        }
    }
}

/* Decodes frames from a stream into the stream's sample buffer, feeding mpg123 buffer data.
 * If not enough data to decode (as N data-frames = 1 full-frame) this will exit but be called again. */
static void decode_mpeg_custom_stream(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, int num_stream) {
    size_t bytes_done = 0, bytes_filled, samples_filled;
    size_t stream_size = get_streamfile_size(stream->streamfile);
    int rc, ok;
    mpeg_custom_stream *ms = data->streams[num_stream];

    //;VGM_LOG("MPEG: decode stream%i @ 0x%08lx (filled=%i, used=%i, buffer_full=%i)\n", num_stream, stream->offset, ms->samples_filled, ms->samples_used, data->buffer_full);

    /* wait until samples are depleted, so buffers don't grow too big */
    if (ms->samples_filled - ms->samples_used > 0) {
        return; /* common with multi-streams, as they decode at different rates) */
    }

    /* no samples = reset the counters */
    ms->samples_filled = 0;
    ms->samples_used = 0;

    /* extra EOF check for edge cases when the caller tries to read more samples than possible */
    if (!data->buffer_full && stream->offset >= stream_size) {
        VGM_LOG("MPEG: EOF found but more data is requested\n");
        goto decode_fail;
    }


    /* read more raw data (could fill the sample buffer too in some cases, namely EALayer3) */
    if (!data->buffer_full) {
        //;VGM_LOG("MPEG: reading more raw data\n");
        switch(data->type) {
            case MPEG_EAL31:
            case MPEG_EAL31b:
            case MPEG_EAL32P:
            case MPEG_EAL32S:   ok = mpeg_custom_parse_frame_ealayer3(stream, data, num_stream); break;
            case MPEG_AHX:      ok = mpeg_custom_parse_frame_ahx(stream, data); break;
            case MPEG_AWC:      ok = mpeg_custom_parse_frame_awc(stream, data, num_stream); break;
            default:            ok = mpeg_custom_parse_frame_default(stream, data); break;
        }
        if (!ok) {
            VGM_LOG("MPEG: cannot parse frame @ around %lx\n",stream->offset);
            goto decode_fail; /* mpg123 could resync but custom MPEGs wouldn't need that */
        }
        //;VGM_LOG("MPEG: read results: bytes_in_buffer=0x%x, new offset=%lx\n", data->bytes_in_buffer, stream->offset);

        /* parse frame may not touch the buffer (only move offset, or fill the sample buffer) */
        if (data->bytes_in_buffer) {
            data->buffer_full = 1;
            data->buffer_used = 0;
        }
    }


    bytes_filled = sizeof(sample)*ms->samples_filled*data->channels_per_frame;
    /* feed new raw data to the decoder if needed, copy decoded results to frame buffer output */
    if (!data->buffer_used) {
        //;VGM_LOG("MPEG: feed new data and get samples\n");
        rc = mpg123_decode(ms->m,
                data->buffer, data->bytes_in_buffer,
                (unsigned char*)ms->output_buffer + bytes_filled, ms->output_buffer_size - bytes_filled,
                &bytes_done);
        data->buffer_used = 1;
    }
    else {
        //;VGM_LOG("MPEG: get samples from old data\n");
        rc = mpg123_decode(ms->m,
                NULL, 0,
                (unsigned char*)ms->output_buffer + bytes_filled, ms->output_buffer_size - bytes_filled,
                &bytes_done);
    }
    samples_filled = (bytes_done / sizeof(sample) / data->channels_per_frame);

    /* discard for weird features (EALayer3 and PCM blocks, AWC and repeated frames) */
    if (ms->decode_to_discard) {
        size_t bytes_to_discard = 0;
        size_t decode_to_discard = ms->decode_to_discard;
        if (decode_to_discard > samples_filled)
            decode_to_discard = samples_filled;
        bytes_to_discard = sizeof(sample)*decode_to_discard*data->channels_per_frame;

        bytes_done -= bytes_to_discard;
        ms->decode_to_discard -= decode_to_discard;
        ms->samples_used += decode_to_discard;
    }

    /* if no decoding was done bytes_done will be zero */
    ms->samples_filled += samples_filled;

    /* not enough raw data, set flag to request more next time
     * (but only with empty mpg123 buffer, EA blocks wait for all samples decoded before advancing blocks) */
    if (!bytes_done && rc == MPG123_NEED_MORE) {
        //;VGM_LOG("MPEG: need more raw data to get samples (bytes_done=%x)\n", bytes_done);
        data->buffer_full = 0;
    }


    //;VGM_LOG("MPEG: stream samples now=%i, filled=%i)\n\n", ms->samples_filled, samples_filled);
    return;

decode_fail:
    /* 0-fill but continue with other streams */
    bytes_filled = ms->samples_filled*data->channels_per_frame*sizeof(sample);
    memset(ms->output_buffer + bytes_filled, 0, ms->output_buffer_size - bytes_filled);
    ms->samples_filled = (ms->output_buffer_size / data->channels_per_frame / sizeof(sample));
}


/*********/
/* UTILS */
/*********/

void free_mpeg(mpeg_codec_data *data) {
    if (!data)
        return;

    if (!data->custom) {
        mpg123_delete(data->m);
    }
    else {
        int i;
        for (i=0; i < data->streams_size; i++) {
            mpg123_delete(data->streams[i]->m);
            free(data->streams[i]->output_buffer);
            free(data->streams[i]);
        }
        free(data->streams);
    }

    free(data->buffer);
    free(data);

    /* The astute reader will note that a call to mpg123_exit is never
     * made. While is is evilly breaking our contract with mpg123, it
     * doesn't actually do anything except set the "initialized" flag
     * to 0. And if we exit we run the risk of turning it off when
     * someone else in another thread is using it. */
}

void reset_mpeg(VGMSTREAM *vgmstream) {
    off_t input_offset;
    mpeg_codec_data *data = vgmstream->codec_data;

    /* reset multistream */ //todo check if stream offsets are properly reset

    if (!data->custom) {
        /* input_offset is ignored as we can assume it will be 0 for a seek to sample 0 */
        mpg123_feedseek(data->m,0,SEEK_SET,&input_offset);
    }
    else {
        int i;
        /* re-start from 0 */
        for (i=0; i < data->streams_size; i++) {
            mpg123_feedseek(data->streams[i]->m,0,SEEK_SET,&input_offset);
            data->streams[i]->samples_filled = 0;
            data->streams[i]->samples_used = 0;
            data->streams[i]->decode_to_discard = 0;
        }

        data->samples_to_discard = data->skip_samples; /* initial delay */
    }
}

void seek_mpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    off_t input_offset;
    mpeg_codec_data *data = vgmstream->codec_data;

    /* seek multistream */
    if (!data->custom) {
        mpg123_feedseek(data->m, num_sample,SEEK_SET,&input_offset);
        if (vgmstream->loop_ch)
            vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset + input_offset;
    }
    else {
        int i;
        /* re-start from 0 */
        for (i=0; i < data->streams_size; i++) {
            mpg123_feedseek(data->streams[i]->m,0,SEEK_SET,&input_offset);
            data->streams[i]->samples_filled = 0;
            data->streams[i]->samples_used = 0;
            data->streams[i]->decode_to_discard = 0;

            if (vgmstream->loop_ch)
                vgmstream->loop_ch[i].offset = vgmstream->loop_ch[i].channel_start_offset;
        }

        /* manually discard samples, since we don't really know the correct offset */
        data->samples_to_discard = num_sample;
        data->samples_to_discard += data->skip_samples;
    }

    data->buffer_full = 0;
    data->buffer_used = 0;
}

/* resets mpg123 decoder and its internals (with mpg123_open_feed as mpg123_feedseek won't work) */
void flush_mpeg(mpeg_codec_data * data) {
    if (!data)
        return;

    if (!data->custom) {
        /* input_offset is ignored as we can assume it will be 0 for a seek to sample 0 */
        mpg123_open_feed(data->m);
    }
    else {
        int i;
        /* re-start from 0 */
        for (i=0; i < data->streams_size; i++) {
            mpg123_open_feed(data->streams[i]->m);
            data->streams[i]->samples_filled = 0;
            data->streams[i]->samples_used = 0;
            data->streams[i]->decode_to_discard = 0;
        }

        data->samples_to_discard = data->skip_samples; /* initial delay */
    }

    data->bytes_in_buffer = 0;
    data->buffer_full = 0;
    data->buffer_used = 0;
}

long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data *data) {
    /* if not found just return 0 and expect to fail (if used for num_samples) */
    if (!data->custom) {
        struct mpg123_frameinfo mi;
        mpg123_handle *m = data->m;

        if (m == NULL || MPG123_OK != mpg123_info(m, &mi))
            return 0;

        /* We would need to read all VBR frames headers to count samples */
        if (mi.vbr != MPG123_CBR) //maybe abr_rate could be used to get an approx
            return 0;

        return (int64_t)bytes * mi.rate * 8 / (mi.bitrate * 1000);
    }
    else {
        return 0; /* a bit too complex for what is worth */
    }
}

/* disables/enables stderr output, for MPEG known to contain recoverable errors */
void mpeg_set_error_logging(mpeg_codec_data * data, int enable) {
    if (!data->custom) {
        mpg123_param(data->m, MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
    }
    else {
        int i;
        for (i=0; i < data->streams_size; i++) {
            mpg123_param(data->streams[i]->m, MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
        }
    }
}

#endif
