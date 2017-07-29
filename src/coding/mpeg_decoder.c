#include "coding.h"
#include "../util.h"
#include "../vgmstream.h"

#ifdef VGM_USE_MPEG
#include <mpg123.h>
#include "mpeg_decoder.h"


/* TODO list for custom decoder
 * - don't force channel param and get them from frame headers for some types (for MPEG_STANDARD)
 * - use one stream per channel and advance streams as channels are done in case of streams like 2ch+1ch+1ch+2ch (not seen)
 *   (would also need to change sample_buffer copy)
 * - improve validation of channels/samples_per_frame between streams
 * - improve decoded samples to sample buffer copying (very picky with sizes)
 * - use mpg123 stream_buffer, with flags per stream, and call copy to sample_buffer when all streams have some samples
 *   (so it could handle interleaved VBR frames).
 * - AHX type 8 encryption
 * - test encoder delays
 * - improve error handling
 */

/* mostly arbitrary max values */
#define MPEG_DATA_BUFFER_SIZE 0x1000
#define MPEG_MAX_CHANNELS 16
#define MPEG_MAX_STREAM_FRAMES 10

static mpg123_handle * init_mpg123_handle();
static void decode_mpeg_standard(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
static void decode_mpeg_custom(VGMSTREAM * vgmstream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
static void decode_mpeg_custom_stream(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, mpg123_handle *m, int channels, int num_stream);

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


        /* reinit, to ignore the reading we've done so far */
        mpg123_open_feed(main_m);
    }

    return data;

fail:
    free_mpeg(data);
    return NULL;
}


/* Init custom MPEG, with given type and config. */
mpeg_codec_data *init_mpeg_custom_codec_data(STREAMFILE *streamFile, off_t start_offset, coding_t *coding_type, int channels, mpeg_custom_t type, mpeg_custom_config *config) {
    mpeg_codec_data *data = NULL;
    int stream_frames = 1;
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
        case MPEG_EAL32P:
        case MPEG_EAL32S:   ok = 0; break; //ok = mpeg_custom_setup_init_ealayer3(streamFile, start_offset, data, coding_type); break;
        default:            ok = mpeg_custom_setup_init_default(streamFile, start_offset, data, coding_type); break;
    }
    if (!ok)
        goto fail;

    if (channels <= 0 || channels > MPEG_MAX_CHANNELS) goto fail;
    if (channels < data->channels_per_frame) goto fail;

    /* init stream decoders (separate as MPEG frames may need N previous frames from their stream to decode) */
    data->ms_size = channels / data->channels_per_frame;
    data->ms = calloc(sizeof(mpg123_handle *), data->ms_size);
    for (i=0; i < data->ms_size; i++) {
        data->ms[i] = init_mpg123_handle();
        if (!data->ms[i]) goto fail;
    }

    if (stream_frames > MPEG_MAX_STREAM_FRAMES) goto fail;

    /* init stream buffer, big enough for one stream and N frames at a time (will be copied to sample buffer) */
    data->stream_buffer_size = sizeof(sample) * data->channels_per_frame * stream_frames * data->samples_per_frame;
    data->stream_buffer = calloc(sizeof(uint8_t), data->stream_buffer_size);
    if (!data->stream_buffer) goto fail;

    /* init sample buffer, big enough for all streams/channels and N frames at a time */
    data->sample_buffer_size = sizeof(sample) * channels * stream_frames * data->samples_per_frame;
    data->sample_buffer = calloc(sizeof(uint8_t), data->sample_buffer_size);
    if (!data->sample_buffer) goto fail;


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
				memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample));
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
 * Decode custom MPEG, allowing support for: single frames, interleave, mutant frames, multiple streams
 * (1 frame = 1/2ch so Nch = 2ch*N/2 or 1ch*N or 2ch+1ch+2ch+...), etc.
 *
 * Decodes samples per each stream and muxes them into a single internal buffer before copying to outbuf
 * (to make sure channel samples are orderly copied between decode_mpeg calls).
 * decode_mpeg_custom_stream does the main decoding, while this handles layout and copying samples to output.
 */
static void decode_mpeg_custom(VGMSTREAM * vgmstream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0, bytes_max, bytes_to_copy;

    while (samples_done < samples_to_do) {

        if (data->bytes_used_in_sample_buffer < data->bytes_in_sample_buffer) {
            /* copy remaining samples */
            bytes_to_copy = data->bytes_in_sample_buffer - data->bytes_used_in_sample_buffer;
            bytes_max = (samples_to_do - samples_done) * sizeof(sample) * channels;
            if (bytes_to_copy > bytes_max)
                bytes_to_copy = bytes_max;
            memcpy((uint8_t*)(outbuf+samples_done*channels), data->sample_buffer + data->bytes_used_in_sample_buffer, bytes_to_copy);

            /* update samples copied */
            data->bytes_used_in_sample_buffer += bytes_to_copy;
            samples_done += bytes_to_copy / sizeof(sample) / channels;
        }
        else {
            /* fill the internal sample buffer */
            int i;
            data->bytes_in_sample_buffer = 0;
            data->bytes_used_in_sample_buffer = 0;

            /* Handle offsets depending on the data layout (may only use half VGMSTREAMCHANNELs with 2ch streams)
             * With multiple offsets it's expected offsets are set up pointing to the first frame of each stream. */
            for (i=0; i < data->ms_size; i++) {
                switch(data->type) {
                  //case MPEG_LYN:
                    case MPEG_FSB:
                    case MPEG_XVAG:
                        /* multiple offsets, decodes 1 frame per stream until reaching interleave/block_size and skips it */
                        decode_mpeg_custom_stream(&vgmstream->ch[i], data, data->ms[i], channels, i);
                        break;

                  //case MPEG_EA: //?
                    case MPEG_AWC:
                        /* consecutive streams: multiple offsets, decodes 1 frame per stream */
                        decode_mpeg_custom_stream(&vgmstream->ch[i], data, data->ms[i], channels, i);
                        break;

                    default:
                        /* N frames: single offset, decodes all N frames per stream (sample buffer must be big enough for N) */
                        decode_mpeg_custom_stream(&vgmstream->ch[0], data, data->ms[i], channels, i);
                        break;
                }
            }

            /* discard (for looping): 'remove' decoded samples from the buffer */
            if (data->samples_to_discard) {
                size_t bytes_to_discard = data->samples_to_discard * sizeof(sample) * channels;

                /* 'remove' all buffer at most */
                if (bytes_to_discard > data->bytes_in_sample_buffer)
                    bytes_to_discard = data->bytes_in_sample_buffer;

                /* pretend the samples were used up and readjust discard */
                data->bytes_used_in_sample_buffer = bytes_to_discard;
                data->samples_to_discard -= bytes_to_discard / sizeof(sample) / channels;;
            }
        }
    }
}

/* Decodes frames from a stream and muxes samples into a intermediate buffer and moves the stream offsets. */
static void decode_mpeg_custom_stream(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, mpg123_handle *m, int channels, int num_stream) {
    size_t bytes_done = 0;
    size_t stream_size = get_streamfile_size(stream->streamfile);
    int rc, ok;


    /* decode samples from one full-frame (as N data-frames = 1 full-frame) before exiting (to orderly copy to sample buffer) */
    do {
        VGM_LOG("MPEG: new step of stream %i @ 0x%08lx\n", num_stream, stream->offset);
        getchar();

        /* extra EOF check for edge cases when the caller tries to read more samples than possible */
        if (stream->offset >= stream_size) {
            memset(data->stream_buffer, 0, data->stream_buffer_size);
            bytes_done = data->stream_buffer_size;
            break; /* continue with other streams */
        }

        /* read more raw data */
        if (!data->buffer_full) {
            //VGM_LOG("MPEG: reading more raw data\n");
            switch(data->type) {
                case MPEG_EAL31:
                case MPEG_EAL32P:
                case MPEG_EAL32S:   ok = 0; break; //ok = mpeg_custom_parse_frame_ealayer3(stream, data); break;
                case MPEG_AHX:      ok = mpeg_custom_parse_frame_ahx(stream, data); break;
                default:            ok = mpeg_custom_parse_frame_default(stream, data); break;
            }
            /* error/EOF, mpg123 can resync in some cases but custom MPEGs wouldn't need that */
            if (!ok || !data->bytes_in_buffer) {
                VGM_LOG("MPEG: cannot parse frame @ around %lx\n",stream->offset);
                memset(data->stream_buffer, 0, data->stream_buffer_size);
                bytes_done = data->stream_buffer_size;
                break; /* continue with other streams */
            }
            VGM_LOG("MPEG: read results: bytes_in_buffer=0x%x, new offset off=%lx\n", data->bytes_in_buffer, stream->offset);

            data->buffer_full = 1;
            data->buffer_used = 0;
        }

#if 0
            //TODO: in case of EALayer3 with "PCM flag" must put them in buffer
            if (ea_pcm_flag) {
                /* write some samples to data->stream_buffer *before* decoding this frame */
                bytes_done += read_streamfile(data->stream_buffer,offset,num_pcm_samples,stream->streamfile);
            }
#endif
#if 0
            //TODO: FSB sometimes has garbage in the first frames, not sure why/when, no apparent patern
            if (data->custom_type == MPEG_FSB && stream->offset == stream->channel_start_offset) { /* first frame */
                VGM_LOG("MPEG: skip first frame @ %x - %x\n", stream->offset, stream->channel_start_offset);

                data->buffer_full = 0;
                memset(data->stream_buffer, 0, data->stream_buffer_size);
                bytes_done = data->stream_buffer_size;
                break;
            }
#endif

        /* feed new raw data to the decoder if needed, copy decoded results to frame buffer output */
        if (!data->buffer_used) {
            //VGM_LOG("MPEG: feed new data and get samples \n");
            rc = mpg123_decode(m,
                    data->buffer, data->bytes_in_buffer,
                    (unsigned char *)data->stream_buffer /*+bytes_done*/, data->stream_buffer_size,
                    &bytes_done);
            data->buffer_used = 1;
        }
        else {
            //VGM_LOG("MPEG: get samples from old data\n");
            rc = mpg123_decode(m,
                    NULL,0,
                    (unsigned char *)data->stream_buffer /*+bytes_done*/, data->stream_buffer_size,
                    &bytes_done);
        }

        /* not enough raw data, request more */
        if (rc == MPG123_NEED_MORE) {
            //VGM_LOG("MPEG: need more raw data to get samples\n");
            /* (apparently mpg123 can give bytes and request more at the same time and may mess up some calcs, when/how?  */
            VGM_ASSERT(bytes_done > 0, "MPEG: bytes done but decoder requests more data\n");
            data->buffer_full = 0;
            continue;
        }
        //VGM_LOG("MPEG: got samples, bytes_done=0x%x (fsbs=0x%x)\n", bytes_done, data->stream_buffer_size);


        break;
    } while (1);


    /* copy decoded full-frame to intermediate sample buffer, muxing channels
     * (ex stream1: ch1s1 ch1s2, stream2: ch2s1 ch2s2  >  ch1s1 ch2s1 ch1s2 ch2s2) */
    {
        size_t samples_done;
        size_t sz = sizeof(sample);
        int channels_f = data->channels_per_frame;
        int fch, i;

        samples_done = bytes_done / sz / channels_f;
        for (fch = 0; fch < channels_f; fch++) { /* channels inside the frame */
            for (i = 0; i < samples_done; i++) { /* decoded samples */
                off_t in_offset = sz*i*channels_f + sz*fch;
                off_t out_offset = sz*i*channels + sz*(num_stream*channels_f + fch);
                memcpy(data->sample_buffer + out_offset, data->stream_buffer + in_offset, sz);
            }
        }

        data->bytes_in_sample_buffer += bytes_done;
    }
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
        for (i=0; i < data->ms_size; i++) {
            mpg123_delete(data->ms[i]);
        }
        free(data->ms);

        free(data->stream_buffer);
        free(data->sample_buffer);
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
        for (i=0; i < data->ms_size; i++) {
            mpg123_feedseek(data->ms[i],0,SEEK_SET,&input_offset);
        }

        data->bytes_in_sample_buffer = 0;
        data->bytes_used_in_sample_buffer = 0;

        /* initial delay */
        data->samples_to_discard = data->skip_samples;
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
        for (i=0; i < data->ms_size; i++) {
            mpg123_feedseek(data->ms[i],0,SEEK_SET,&input_offset);
            if (vgmstream->loop_ch)
                vgmstream->loop_ch[i].offset = vgmstream->loop_ch[i].channel_start_offset;
        }
        data->bytes_in_sample_buffer = 0;
        data->bytes_used_in_sample_buffer = 0;

        /* manually discard samples, since we don't really know the correct offset */
        data->samples_to_discard = num_sample;
        data->samples_to_discard += data->skip_samples;
    }

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

/* disables/enables stderr output, useful for MPEG known to contain recoverable errors */
void mpeg_set_error_logging(mpeg_codec_data * data, int enable) {
    if (!data->custom) {
        mpg123_param(data->m, MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
    }
    else {
        int i;
        for (i=0; i < data->ms_size; i++) {
            mpg123_param(data->ms[i], MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
        }
    }
}

#endif
