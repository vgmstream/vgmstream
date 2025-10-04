#ifdef VGM_USE_MPEG
#include <mpg123.h>
#include "coding.h"
#include "../vgmstream.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "mpeg_decoder.h"


#define MPEG_DATA_BUFFER_SIZE 0x1000 // at least one MPEG frame (max ~0x5A1 plus some more in case of free bitrate)
#define MPEG_MAX_CHANNELS 16 // arbitrary max


static void free_mpeg(void* priv_data) {
    mpeg_codec_data* data = priv_data;
    if (!data)
        return;

    if (!data->custom) {
        mpg123_delete(data->handle);
    }
    else {
        for (int i = 0; i < data->streams_count; i++) {
            if (!data->streams)
                continue;
            mpg123_delete(data->streams[i].handle);
            free(data->streams[i].buffer);
            free(data->streams[i].sbuf);
        }
        free(data->streams);
    }

    free(data->buffer);
    free(data->sbuf);
    free(data);
}

static mpg123_handle* init_mpg123_handle(void) {
    mpg123_handle* handle = NULL;
    int rc;

    // in old versions it was needed to call mpg123_init()
    if (MPG123_API_VERSION <= 46)
        goto fail;

    handle = mpg123_new(NULL, &rc);
    if (rc != MPG123_OK) goto fail;

    mpg123_param(handle, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0);
    mpg123_param(handle, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0.0); // wonky support
    mpg123_param(handle, MPG123_RESYNC_LIMIT, -1, 0x2000); // just in case, games shouldn't need this
#ifndef VGM_DEBUG_OUTPUT
    mpg123_param(handle, MPG123_ADD_FLAGS, MPG123_QUIET, 1);
#endif

    rc = mpg123_open_feed(handle);
    if (rc != MPG123_OK) goto fail;

    return handle;

fail:
    mpg123_delete(handle);
    return NULL;
}


/* Inits regular MPEG */
mpeg_codec_data* init_mpeg(STREAMFILE* sf, off_t start_offset, coding_t* coding_type, int channels) {
    mpeg_codec_data* data = NULL;

    /* init codec */
    data = calloc(1, sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DATA_BUFFER_SIZE;
    data->buffer = calloc(data->buffer_size, sizeof(uint8_t));
    if (!data->buffer) goto fail;

    data->handle = init_mpg123_handle();
    if (!data->handle) goto fail;


    /* check format */
    {
        int bytes_read = read_streamfile(data->buffer, start_offset, data->buffer_size, sf);
        // don't check max as sfx can be smaller than buffer

        // start_offset should be correct but just in case, read first frame(s)
        int rc;
        int pos = 0;
        do {
            size_t bytes_done;
            rc = mpg123_decode(data->handle, data->buffer + pos, bytes_read, NULL,0, &bytes_done);
            if (rc != MPG123_OK && rc != MPG123_NEW_FORMAT && rc != MPG123_NEED_MORE) {
                VGM_LOG("MPEG: unable to set up mpg123 at start offset\n");
                goto fail; //handle MPG123_DONE?
            }
            if (bytes_read <= 0x10) { /* don't hang in some incorrectly detected formats */
                VGM_LOG("MPEG: unable to find mpeg data at start offset\n");
                goto fail;
            }

            pos++;
            bytes_read--;
        }
        while (rc != MPG123_NEW_FORMAT);
    }

    {
        int samples_per_frame;
        long sample_rate_per_frame;
        int channels_per_frame, encoding;
        struct mpg123_frameinfo mi = {0};

        /* check first frame header and validate */
        int rc = mpg123_getformat(data->handle, &sample_rate_per_frame, &channels_per_frame, &encoding);
        if (rc != MPG123_OK) goto fail;

        mpg123_info(data->handle, &mi);

        if (encoding != MPG123_ENC_FLOAT_32)
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
        else
            goto fail;

        data->channels_per_frame = channels_per_frame;
        data->samples_per_frame = samples_per_frame;
        if (!data->sample_rate)
            data->sample_rate = mi.rate;
        if (!data->bitrate)
            data->bitrate = mi.bitrate;
        data->is_vbr = mi.vbr != MPG123_CBR;

        // reinit, to ignore the reading done
        mpg123_open_feed(data->handle);
    }

    data->sbuf_size = sizeof(float) * channels * data->samples_per_frame;
    data->sbuf = calloc(data->sbuf_size, sizeof(uint8_t));
    if (!data->sbuf) goto fail;

    return data;

fail:
    free_mpeg(data);
    return NULL;
}


/* Init custom MPEG, with given type and config */
mpeg_codec_data* init_mpeg_custom(STREAMFILE* sf, off_t start_offset, coding_t* coding_type, int channels, mpeg_custom_t type, mpeg_custom_config* cfg) {
    mpeg_codec_data* data = NULL;
    int ok;

    /* init codec */
    data = calloc(1, sizeof(mpeg_codec_data));
    if (!data) goto fail;

    /* keep around to decode */
    data->custom = true;
    data->type = type;
    if (cfg)
        memcpy(&data->config, cfg, sizeof(mpeg_custom_config));
    data->config.channels = channels;

    data->default_buffer_size = MPEG_DATA_BUFFER_SIZE;

    /* init per subtype */
    switch(data->type) {
        case MPEG_EAL31:
        case MPEG_EAL31b:
        case MPEG_EAL32P:
        case MPEG_EAL32S:   ok = mpeg_custom_setup_init_ealayer3(sf, start_offset, data, coding_type); break;
        case MPEG_EAMP3:    ok = mpeg_custom_setup_init_eamp3(sf, start_offset, data, coding_type); break;
        default:            ok = mpeg_custom_setup_init_default(sf, start_offset, data, coding_type); break;
    }
    if (!ok)
        goto fail;

    if (channels < 1 || channels > MPEG_MAX_CHANNELS)
        goto fail;
    if (channels < data->channels_per_frame)
        goto fail;

    //todo simplify/unify XVAG/P3D/SCD/LYN and just feed arbitrary chunks to the decoder
    /* max for some Ubi Lyn */
    if (data->default_buffer_size > 0x20000) {
        VGM_LOG("MPEG: buffer size too big %x\n", data->default_buffer_size);
        goto fail;
    }


    /* init streams */
    data->streams_count = channels / data->channels_per_frame;

    /* 2ch streams + odd channels = last stream must be 1ch */
    /* (known channels combos are 2ch+..+2ch, 1ch+..+1ch, or rarely 2ch+..+2ch+1ch in EALayer3) */
    if (data->channels_per_frame == 2 && channels % 2)
        data->streams_count += 1;

    data->streams = calloc(data->streams_count, sizeof(mpeg_custom_stream));
    if (!data->streams) goto fail;

    for (int i = 0; i < data->streams_count; i++) {
        //data->streams[i] = calloc(1, sizeof(mpeg_custom_stream));
        //if (!data->streams[i]) goto fail;
        data->streams[i].handle = init_mpg123_handle(); /* decoder not shared as frames depend on prev state */
        if (!data->streams[i].handle) goto fail;

        /* size could be any value */
        data->streams[i].sbuf_size = sizeof(float) * data->channels_per_frame * data->samples_per_frame;
        data->streams[i].sbuf = calloc(data->streams[i].sbuf_size, sizeof(uint8_t));
        if (!data->streams[i].sbuf) goto fail;

        /* one per stream as sometimes mpg123 can't read the whole buffer in one pass */
        data->streams[i].buffer_size = data->default_buffer_size;
        data->streams[i].buffer = calloc(data->streams[i].buffer_size, sizeof(uint8_t));
        if (!data->streams[i].buffer) goto fail;

        data->streams[i].channels_per_frame = data->channels_per_frame;
        if (i + 1 == data->streams_count && data->channels_per_frame == 2 && channels % 2)
            data->streams[i].channels_per_frame = 1;
    }

    data->sbuf_size = sizeof(float) * channels * data->samples_per_frame;
    data->sbuf = calloc(data->sbuf_size, sizeof(uint8_t));
    if (!data->sbuf) goto fail;

    return data;

fail:
    free_mpeg(data);
    return NULL;
}


/************/
/* DECODERS */
/************/

/**
 * Decode anything mpg123 can.
 * Feeds raw data and extracts decoded samples as needed.
 */
static void decode_mpeg_standard(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, float* sbuf, int32_t samples_to_do, int channels) {

    int samples_done = 0;
    while (samples_done < samples_to_do) {
        size_t bytes_done;
        int rc, bytes_to_do;

        /* read more raw data */
        if (!data->buffer_full) {
            data->bytes_in_buffer = read_streamfile(data->buffer, stream->offset, data->buffer_size, stream->streamfile);

            /* end of stream, fill rest with 0s */
            if (data->bytes_in_buffer <= 0) {
                int samples_left = samples_to_do - samples_done;
                VGM_ASSERT(samples_left, "MPEG: end of stream, filling %i\n", samples_left);
                memset(sbuf, 0, samples_left * channels * sizeof(float));
                break;
            }

            data->buffer_full = true;
            data->buffer_used = false;

            stream->offset += data->bytes_in_buffer;
        }

        bytes_to_do = (samples_to_do-samples_done) * channels * sizeof(float);

        /* feed new raw data to the decoder if needed, copy decoded results to output */
        if (!data->buffer_used) {
            rc = mpg123_decode(data->handle, data->buffer, data->bytes_in_buffer, sbuf, bytes_to_do, &bytes_done);
            data->buffer_used = true;
        }
        else {
            rc = mpg123_decode(data->handle, NULL, 0, sbuf, bytes_to_do, &bytes_done);
        }

        /* not enough raw data, request more */
        if (rc == MPG123_NEED_MORE) {
            data->buffer_full = false;
        }
        VGM_ASSERT(rc != MPG123_NEED_MORE && rc != MPG123_OK, "MPEG: error %i\n", rc);

        /* update copied samples */
        samples_done += bytes_done / sizeof(float) / channels;
        sbuf += bytes_done / sizeof(float);
    }
}


/* Decodes frames from a stream into the stream's sample buffer, feeding mpg123 buffer data.
 * If not enough data to decode (as N data-frames = 1 full-frame) this will exit but be called again. */
static void decode_mpeg_custom_stream(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream) {
    size_t bytes_done = 0, bytes_filled, samples_filled;
    size_t stream_size = get_streamfile_size(stream->streamfile);
    int rc, ok;
    mpeg_custom_stream* ms = &data->streams[num_stream];
    int channels_per_frame = ms->channels_per_frame;
    float* sbuf = ms->sbuf;

    //;VGM_LOG("MPEG: decode stream%i @ 0x%08lx (filled=%i, used=%i, buffer_full=%i)\n", num_stream, stream->offset, ms->samples_filled, ms->samples_used, ms->buffer_full);

    /* wait until samples are depleted, so buffers don't grow too big */
    if (ms->samples_filled - ms->samples_used > 0) {
        return; /* common with multi-streams, as they decode at different rates) */
    }

    /* no samples = reset the counters */
    ms->samples_filled = 0;
    ms->samples_used = 0;

    /* extra EOF check for edge cases when the caller tries to read more samples than possible */
    if (!ms->buffer_full && stream->offset >= stream_size) {
        VGM_LOG("MPEG: EOF found but more data is requested in stream %i\n", num_stream);
        goto decode_fail;
    }


    /* read more raw data (could fill the sample buffer too in some cases, namely EALayer3) */
    if (!ms->buffer_full) {
        //;VGM_LOG("MPEG: reading more raw data\n");
        switch(data->type) {
            case MPEG_EAL31:
            case MPEG_EAL31b:
            case MPEG_EAL32P:
            case MPEG_EAL32S:   ok = mpeg_custom_parse_frame_ealayer3(stream, data, num_stream); break;
            case MPEG_AHX:      ok = mpeg_custom_parse_frame_ahx(stream, data, num_stream); break;
            case MPEG_EAMP3:    ok = mpeg_custom_parse_frame_eamp3(stream, data, num_stream); break;
            default:            ok = mpeg_custom_parse_frame_default(stream, data, num_stream); break;
        }
        if (!ok) {
            VGM_LOG_ONCE("MPEG: cannot parse frame @ around %x\n",(uint32_t)stream->offset);
            goto decode_fail; /* mpg123 could resync but custom MPEGs wouldn't need that */
        }
        //;VGM_LOG("MPEG: read results: bytes_in_buffer=0x%x, new offset=%lx\n", ms->bytes_in_buffer, stream->offset);

        /* parse frame may not touch the buffer (only move offset, or fill the sample buffer) */
        if (ms->bytes_in_buffer) {
            ms->buffer_full = true;
            ms->buffer_used = false;
        }
    }

    sbuf += ms->samples_filled * channels_per_frame;
    bytes_filled = sizeof(float) * ms->samples_filled * channels_per_frame;
    /* feed new raw data to the decoder if needed, copy decoded results to frame buffer output */
    if (!ms->buffer_used) {
        //;VGM_LOG("MPEG: feed new data and get samples\n");
        rc = mpg123_decode(ms->handle, ms->buffer, ms->bytes_in_buffer, sbuf, ms->sbuf_size - bytes_filled, &bytes_done);
        ms->buffer_used = true;
    }
    else {
        //;VGM_LOG("MPEG: get samples from old data\n");
        rc = mpg123_decode(ms->handle, NULL, 0, sbuf, ms->sbuf_size - bytes_filled, &bytes_done);
    }
    samples_filled = bytes_done / channels_per_frame / sizeof(float);

    /* discard for weird features (EALayer3 and PCM blocks, AWC and repeated frames) */
    if (ms->decode_to_discard) {
        size_t decode_to_discard = ms->decode_to_discard;
        if (decode_to_discard > samples_filled)
            decode_to_discard = samples_filled;
        size_t bytes_to_discard = sizeof(float) * decode_to_discard * channels_per_frame;

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
        ms->buffer_full = false;
    }


    //;VGM_LOG("MPEG: stream samples now=%i, filled=%i)\n\n", ms->samples_filled, samples_filled);
    return;

decode_fail:
    /* 0-fill but continue with other streams */
    bytes_filled = ms->samples_filled * channels_per_frame * sizeof(float);
    memset(sbuf + bytes_filled, 0, ms->sbuf_size - bytes_filled);
    ms->samples_filled = (ms->sbuf_size / channels_per_frame / sizeof(float));
}

/**
 * Decode custom MPEG, for: single frames, mutant frames, interleave/multiple streams (Nch = 2ch*N/2 or 1ch*N), etc.
 *
 * Copies to outbuf when there are samples in all streams and calls decode_mpeg_custom_stream to decode.
 . Depletes the stream's sample buffers before decoding more, so it doesn't run out of buffer space.
 */
static void decode_mpeg_custom(VGMSTREAM* vgmstream, mpeg_codec_data* data, float* sbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;

    while (samples_done < samples_to_do) {
        int samples_to_copy = -1;

        /* find max to copy from all streams (equal for all channels) */
        for (int i = 0; i < data->streams_count; i++) {
            size_t samples_in_stream = data->streams[i].samples_filled - data->streams[i].samples_used;
            if (samples_to_copy < 0 || samples_in_stream < samples_to_copy)
                samples_to_copy = samples_in_stream;
        }


        /* discard if needed (for looping) */
        if (data->samples_to_discard) {
            int samples_to_discard = samples_to_copy;
            if (samples_to_discard > data->samples_to_discard)
                samples_to_discard = data->samples_to_discard;

            for (int i = 0; i < data->streams_count; i++) {
                data->streams[i].samples_used += samples_to_discard;
            }
            data->samples_to_discard -= samples_to_discard;
            samples_to_copy -= samples_to_discard;
        }

        /* mux streams channels (1/2ch combos) to sbuf (Nch) */
        if (samples_to_copy > 0) {
            if (samples_to_copy > samples_to_do - samples_done)
                samples_to_copy = samples_to_do - samples_done;

            int ch = 0;
            for (int stream = 0; stream < data->streams_count; stream++) {
                mpeg_custom_stream* ms = &data->streams[stream];
                int stream_channels = ms->channels_per_frame;

                for (int stream_ch = 0; stream_ch < stream_channels; stream_ch++) {
                    for (int s = 0; s < samples_to_copy; s++) {
                        size_t stream_sample = (ms->samples_used+s)*stream_channels + stream_ch;
                        size_t buffer_sample = (samples_done+s)*channels + ch;

                        sbuf[buffer_sample] = ms->sbuf[stream_sample];
                    }
                    ch++;
                }

                ms->samples_used += samples_to_copy;
            }

            samples_done += samples_to_copy;
        }
        else {
            /* decode more into stream sample buffers */

            /* Handle offsets depending on the data layout (may only use half VGMSTREAMCHANNELs with 2ch streams)
             * With multiple offsets they should already start in the first frame of each stream. */
            for (int i = 0; i < data->streams_count; i++) {
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

static bool decode_frame_mpeg(VGMSTREAM* v) {
    mpeg_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;

    // TODO: needed for EALayer3, that has block with max number of samples (could be handled by reading single frames)
    int samples_to_do = ds->samples_left;
    if (samples_to_do > data->samples_per_frame)
        samples_to_do = data->samples_per_frame;

    if (!data->custom) {
        decode_mpeg_standard(&v->ch[0], data, data->sbuf, samples_to_do, v->channels);
    } else {
        decode_mpeg_custom(v, data, data->sbuf, samples_to_do, v->channels);
    }

    sbuf_init_flt(&ds->sbuf, data->sbuf, samples_to_do, v->channels);
    ds->sbuf.filled = samples_to_do;

    return true;
}

/*********/
/* UTILS */
/*********/

/* resets mpg123 decoder and its internals without seeking, useful when a new MPEG substream starts */
static void flush_mpeg(mpeg_codec_data* data, int is_loop) {
    if (!data)
        return;

    if (!data->custom) {
        /* input_offset is ignored as we can assume it will be 0 for a seek to sample 0 */
        mpg123_open_feed(data->handle); // mpg123_feedseek won't work
    }
    else {
        /* re-start from 0 */
        for (int i = 0; i < data->streams_count; i++) {
            if (!data->streams)
                continue;

            /* On loop FSB retains MDCT state so it mixes with next/loop frame (confirmed with recordings).
             * This only matters on full loops and if there is no encoder delay (since loops use discard right now) */
            if (is_loop && data->custom && !(data->type == MPEG_FSB))
                mpg123_open_feed(data->streams[i].handle);
            data->streams[i].bytes_in_buffer = 0;
            data->streams[i].buffer_full = false;
            data->streams[i].buffer_used = false;
            data->streams[i].samples_filled = 0;
            data->streams[i].samples_used = 0;
            data->streams[i].current_size_count = 0;
            data->streams[i].current_size_target = 0;
            data->streams[i].decode_to_discard = 0;
        }

        data->samples_to_discard = data->skip_samples;
    }

    data->bytes_in_buffer = 0;
    data->buffer_full = false;
    data->buffer_used = false;
}

static void reset_mpeg(void* priv_data) {
    mpeg_codec_data* data = priv_data;
    if (!data) return;

    flush_mpeg(data, 0);

#if 0
    /* flush_mpeg properly resets mpg123 with mpg123_open_feed, and
     * offsets are reset in the VGMSTREAM externally, but for posterity: */
    if (!data->custom) {
        off_t input_offset = 0;
        mpg123_feedseek(data->handle,0,SEEK_SET,&input_offset);
    }
    else {
        off_t input_offset = 0;
        int i;
        for (i = 0; i < data->streams_count; i++) {
            if (!data->streams)
                continue;
            mpg123_feedseek(data->streams[i].handle,0,SEEK_SET,&input_offset);
        }
    }
#endif
}

/* seeks to a point */
static void seek_mpeg(VGMSTREAM* v, int32_t num_sample) {
    mpeg_codec_data* data = v->codec_data;
    if (!data) return;


    if (!data->custom) {
        off_t input_offset = 0;

        mpg123_feedseek(data->handle, num_sample, SEEK_SET, &input_offset);

        /* adjust loop with mpg123's offset (useful?) */
        if (v->loop_ch)
            v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset + input_offset;
    }
    else {
        flush_mpeg(data, 1);

        /* restart from 0 and manually discard samples, since we don't really know the correct offset */
        for (int i = 0; i < data->streams_count; i++) {
            //if (!data->streams)
            //    continue;
            //mpg123_feedseek(data->streams[i].handle,0,SEEK_SET,&input_offset); /* already reset */

            /* force first offset as discard-looping needs to start from the beginning */
            if (v->loop_ch)
                v->loop_ch[i].offset = v->loop_ch[i].channel_start_offset;
        }

        data->samples_to_discard += num_sample;
    }
}


int mpeg_get_sample_rate(mpeg_codec_data* data) {
    return data->sample_rate;
}

long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data* data) {
    /* if not found just return 0 and expect to fail (if used for num_samples) */
    if (!data->custom) {
        /* We would need to read all VBR frames headers to count samples */
        if (data->is_vbr) { //maybe abr_rate could be used to get an approx
            VGM_LOG("MPEG: vbr mp3 can't do bytes_to_samples\n");
            return 0;
        }

        return (int64_t)bytes * data->sample_rate * 8 / (data->bitrate * 1000);
    }
    else {
        /* needed for SCD */
        if (data->streams_count && data->bitrate) {
            return (int64_t)(bytes / data->streams_count) * data->sample_rate * 8 / (data->bitrate * 1000);
        }

        return 0;
    }
}

#if 0
/* disables/enables stderr output, for MPEG known to contain recoverable errors */
void mpeg_set_error_logging(mpeg_codec_data* data, int enable) {
    if (!data->custom) {
        mpg123_param(data->handle, MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
    }
    else {
        int i;
        for (i=0; i < data->streams_count; i++) {
            mpg123_param(data->streams[i].handle, MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
        }
    }
}
#endif

const codec_info_t mpeg_decoder = {
    .sample_type = SFMT_FLT,
    .decode_frame = decode_frame_mpeg,
    .free = free_mpeg,
    .reset = reset_mpeg,
    .seek = seek_mpeg,
};
#endif
