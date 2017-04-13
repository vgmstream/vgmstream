#include "coding.h"
#include "../util.h"
#include "../vgmstream.h"

#ifdef VGM_USE_MPEG
#include <mpg123.h>

#define AHX_EXPECTED_FRAME_SIZE 0x414
#define MPEG_DEFAULT_BUFFER_SIZE 0x1000  /* should be >= AHX_EXPECTED_FRAME_SIZE */

typedef struct {
    int version;
    int layer;
    int bit_rate;
    int sample_rate;
    int frame_samples;
    int frame_size; /* bytes */
    int channels;
    coding_t coding_type;
} mpeg_frame_info;

static mpg123_handle * init_mpg123_handle();
static void decode_mpeg_default(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
static void decode_mpeg_interleave(VGMSTREAM * vgmstream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
static void decode_mpeg_interleave_samples(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, mpg123_handle *m, int channels, int num_stream, size_t block_size);
static int update_frame_sizes(mpeg_codec_data * data, STREAMFILE *streamfile, off_t offset);
static int mpeg_get_frame_info(STREAMFILE *streamfile, off_t offset, mpeg_frame_info * info);

/**
 * Inits regular MPEG.
 */
mpeg_codec_data *init_mpeg_codec_data(STREAMFILE *streamfile, off_t start_offset, coding_t *coding_type, int channels) {
    mpeg_codec_data *data = NULL;

    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DEFAULT_BUFFER_SIZE;
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
                goto fail; //todo handle MPG123_DONE?
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

        if (mi.version == MPG123_1_0 && mi.layer == 1)
            *coding_type = coding_MPEG1_L1;
        else if (mi.version == MPG123_1_0 && mi.layer == 2)
            *coding_type = coding_MPEG1_L2;
        else if (mi.version == MPG123_1_0 && mi.layer == 3)
            *coding_type = coding_MPEG1_L3;
        else if (mi.version == MPG123_2_0 && mi.layer == 1)
            *coding_type = coding_MPEG2_L1;
        else if (mi.version == MPG123_2_0 && mi.layer == 2)
            *coding_type = coding_MPEG2_L2;
        else if (mi.version == MPG123_2_0 && mi.layer == 3)
            *coding_type = coding_MPEG2_L3;
        else if (mi.version == MPG123_2_5 && mi.layer == 1)
            *coding_type = coding_MPEG25_L1;
        else if (mi.version == MPG123_2_5 && mi.layer == 2)
            *coding_type = coding_MPEG25_L2;
        else if (mi.version == MPG123_2_5 && mi.layer == 3)
            *coding_type = coding_MPEG25_L3;
        else goto fail;

        if (mi.layer == 1)
            samples_per_frame = 384;
        else if (mi.layer == 2)
            samples_per_frame = 1152;
        else if (mi.layer == 3 && mi.version == MPG123_1_0) //MP3
            samples_per_frame = 1152;
        else if (mi.layer == 3)
            samples_per_frame = 576;
        else goto fail;

        data->sample_rate_per_frame = sample_rate_per_frame;
        data->channels_per_frame = channels_per_frame;
        data->samples_per_frame = samples_per_frame;

        /* reinit, to ignore the reading we've done so far */
        mpg123_open_feed(main_m);
    }

    return data;

fail:
    free_mpeg(data);
    return NULL;
}

/**
 * Init interleaved MPEG (also accepts normal MPEGs, but it's less error tolerant than normal MPEG init).
 */
mpeg_codec_data *init_mpeg_codec_data_interleaved(STREAMFILE *streamfile, off_t start_offset, coding_t *coding_type, int channels, int fixed_frame_size, int fsb_padding) {
    mpeg_codec_data *data = NULL;

    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;

    data->m = init_mpg123_handle();
    if (!data->m) goto fail;

    /* interleaved setup */
    {
        mpeg_frame_info info;
        int i;


        data->interleaved = 1;

        /* get frame info at offset */
        if ( !mpeg_get_frame_info(streamfile, start_offset, &info) )
            goto fail;

        *coding_type = info.coding_type;
        data->channels_per_frame = info.channels;
        data->sample_rate_per_frame = info.sample_rate;
        data->samples_per_frame = info.frame_samples;

        data->fixed_frame_size = fixed_frame_size;
        data->current_frame_size = fixed_frame_size;
        data->fsb_padding = fsb_padding;

        /* get frame size from the first frame as it can be used externally to calc interleave */
        if ( !update_frame_sizes(data, streamfile, start_offset) )
            goto fail;
//VGM_LOG("ch=%i, sr=%i, spf=%i, v=%i, l=%i\n", data->channels_per_frame, data->sample_rate_per_frame, data->samples_per_frame, info.version, info.layer);

//mpg123_handle *main_m = data->m;
//struct mpg123_frameinfo mi;
//mpg123_info(main_m,&mi);
//VGM_LOG("mi.framesize=%x\n", mi.framesize);
        /* unlikely, can fixed with bigger buffer or a feed loop */
        if (info.frame_size > data->buffer_size)
            goto fail;

        if (channels < 1 || channels > 32) goto fail; /* arbitrary max */
        if (channels < data->channels_per_frame) goto fail;

        //todo improve (less buffers / simplify data->ms)
        /* Init interleaved audio, which needs separate decoders per stream and frame size stuff.
         * We still leave data->m as a "base" info/format to simplify some stuff */
        data->ms_size = channels / data->channels_per_frame;
        data->ms = calloc(sizeof(mpg123_handle *), data->ms_size);
        for (i=0; i < data->ms_size; i++) {
            data->ms[i] = init_mpg123_handle();
            if (!data->ms[i]) goto fail;
        }

        data->frame_buffer_size = sizeof(sample) * data->samples_per_frame * data->channels_per_frame;
        data->frame_buffer = calloc(sizeof(uint8_t), data->frame_buffer_size);
        if (!data->frame_buffer) goto fail;

        data->interleave_buffer_size = sizeof(sample) * data->samples_per_frame * channels;
        data->interleave_buffer = calloc(sizeof(uint8_t), data->interleave_buffer_size);
        if (!data->interleave_buffer) goto fail;
    }


    return data;

fail:
    free_mpeg(data);
    return NULL;
}


/**
 * Inits MPEG for AHX, which ignores frame headers.
 */
mpeg_codec_data *init_mpeg_codec_data_ahx(STREAMFILE *streamFile, off_t start_offset, int channel_count) {
    mpeg_codec_data *data = NULL;

    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;

    data->m = init_mpg123_handle();
    if (!data->m) goto fail;


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

    mpg123_param(m,MPG123_REMOVE_FLAGS,MPG123_GAPLESS,0.0); //todo fix gapless
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

    /* MPEGs streams contain one or two channels, so we may only need half VGMSTREAMCHANNELs for offsets */
    if (data->interleaved) {
        decode_mpeg_interleave(vgmstream, data, outbuf, samples_to_do, channels);
    } else {
        decode_mpeg_default(&vgmstream->ch[0], data, outbuf, samples_to_do, channels);
    }
}

/**
 * Decode anything mpg123 can.
 * Feeds raw data and extracts decoded samples as needed.
 */
static void decode_mpeg_default(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
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
        } else {
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
 * Decode interleaved (multichannel) MPEG. Works with mono/stereo too.
 * Channels (1 or 2), samples and frame size per stream should be constant.
 *
 * Reads frame 'streams' (ex. 4ch = 1+1+1+1 = 4 streams or 2+2 = 2 streams), decodes
 * samples per stream and muxes them into a single internal buffer before copying to outbuf
 * (to make sure channel samples are orderly copied between decode_mpeg calls).
 *
 * Interleave variations:
 * - blocks of frames: fixed block_size per stream (unknown number of samples) [XVAG]
 *   (ex. b1 = N samples of ch1, b2 = N samples of ch2, b3 = M samples of ch1, etc)
 * - partial frames: single frames per stream with padding (block_size is frame_size+padding) [FSB]
 *   (ex. f1+f3+f5 = 1152*2 samples of ch1+2, f2+f4 = 1152*2 samples of ch3+4, etc)
 */
static void decode_mpeg_interleave(VGMSTREAM * vgmstream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0, bytes_max, bytes_to_copy;

    while (samples_done < samples_to_do) {

        if (data->bytes_used_in_interleave_buffer < data->bytes_in_interleave_buffer) {
            /* copy remaining samples */
            bytes_to_copy = data->bytes_in_interleave_buffer - data->bytes_used_in_interleave_buffer;
            bytes_max = (samples_to_do - samples_done) * sizeof(sample) * channels;
            if (bytes_to_copy > bytes_max)
                bytes_to_copy = bytes_max;
            memcpy((uint8_t*)(outbuf+samples_done*channels), data->interleave_buffer + data->bytes_used_in_interleave_buffer, bytes_to_copy);

            /* update samples copied */
            data->bytes_used_in_interleave_buffer += bytes_to_copy;
            samples_done += bytes_to_copy / sizeof(sample) / channels;
        }
        else {
            /* fill the internal sample buffer */
            int i;
            data->bytes_in_interleave_buffer = 0;
            data->bytes_used_in_interleave_buffer = 0;

            for (i=0; i < data->ms_size; i++) {
                decode_mpeg_interleave_samples(&vgmstream->ch[i], data, data->ms[i], channels, i, vgmstream->interleave_block_size);
            }

            /* discard (for looping): 'remove' decoded samples from the buffer */
            if (data->samples_to_discard) {
                size_t bytes_to_discard = data->samples_to_discard * sizeof(sample) * channels;

                /* 'remove' all buffer at most */
                if (bytes_to_discard > data->bytes_in_interleave_buffer)
                    bytes_to_discard = data->bytes_in_interleave_buffer;

                /* pretend the samples were used up */
                data->bytes_used_in_interleave_buffer = bytes_to_discard;

                /* and readjust discard */
                data->samples_to_discard -= bytes_to_discard / sizeof(sample) / channels;;
            }
        }
    }

}

/**
 * Decodes frames from a stream and muxes the samples into a intermediate buffer.
 * Skips to the next interleaved block once reaching the stream's block end.
 */
static void decode_mpeg_interleave_samples(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, mpg123_handle *m, int channels, int num_stream, size_t block_size) {
    size_t bytes_done;
    size_t stream_size =  get_streamfile_size(stream->streamfile);

    /* decode samples from 1 full frame */
    do {
        int rc;
        //int first_frame = stream->offset == stream->channel_start_offset;

        /* padded frame stuff */
        rc = update_frame_sizes(data, stream->streamfile, stream->offset);
        /* ignore any errors and continue; mpg123 will probably sync */
        VGM_ASSERT(rc==0, "MPEG: frame error @ 0x%08lx (prev size=0x%x / padding=0x%x)\n", stream->offset, data->current_frame_size, data->current_padding);




        /* extra EOF check for edge cases when the caller tries to read more samples than possible */
        if (stream->offset > stream_size) {
            memset(data->frame_buffer, 0, data->frame_buffer_size);
            bytes_done = data->frame_buffer_size;
            break;
        }


        /* read more raw data (only 1 frame, to check interleave block end) */
        if (!data->buffer_full) {
            data->bytes_in_buffer = read_streamfile(data->buffer,stream->offset,data->current_frame_size,stream->streamfile);
            //VGM_LOG("read raw: cfs=%x, cp=%x, tot=%x, off=%x\n", data->current_frame_size, data->current_padding, data->current_frame_size + data->current_padding, stream->offset);

            /* end of stream, fill frame buffer with 0s but continue normally with other streams */
            if (!data->bytes_in_buffer) {
                memset(data->frame_buffer, 0, data->frame_buffer_size);
                bytes_done = data->frame_buffer_size;
                break;
            }

            data->buffer_full = 1;
            data->buffer_used = 0;

            stream->offset += data->current_frame_size + data->current_padding; /* skip FSB frame+garbage */
            if (block_size && ((stream->offset - stream->channel_start_offset) % block_size==0)) {
                stream->offset += block_size * (data->ms_size-1); /* skip a block per stream if block done */
            }

            /*
            /// TODO: skip very first frame but add fake silence?
            //todo skip only if first frame is fully decodable? (ie.- blank below)
            //todo for multich files idem
            if (first_frame) {
                data->buffer_full = 0;

                VGM_LOG("skip first frame @ %x - %x\n", stream->offset, stream->channel_start_offset);
                memset(data->frame_buffer, 0, data->frame_buffer_size);
                bytes_done = data->frame_buffer_size;
                break;
            }
            */
        }


        /* feed new raw data to the decoder if needed, copy decoded results to frame buffer output */
        if (!data->buffer_used) {
            //VGM_LOG("feed unused: cfs=%x, cp=%x, tot=%x, off=%x\n", data->current_frame_size, data->current_padding, data->current_frame_size + data->current_padding, stream->offset);
            //getchar();

            rc = mpg123_decode(m,
                    data->buffer, data->bytes_in_buffer,
                    (unsigned char *)data->frame_buffer, data->frame_buffer_size,
                    &bytes_done);
            data->buffer_used = 1;
        } else {
            //VGM_LOG("feed used: cfs=%x, cp=%x, tot=%x, off=%x\n", data->current_frame_size, data->current_padding, data->current_frame_size + data->current_padding, stream->offset);
            //getchar();

            rc = mpg123_decode(m,
                    NULL,0,
                    (unsigned char *)data->frame_buffer, data->frame_buffer_size,
                    &bytes_done);
        }

        /* samples per frame should be constant... */
        if (bytes_done > 0 && bytes_done < data->frame_buffer_size) {
            VGM_LOG("MPEG: borked frame @ 0x%08lx (%i bytes done, expected %i, rc=%i)\n", stream->offset, bytes_done, data->frame_buffer_size, rc);
            memset(data->frame_buffer + bytes_done, 0, data->frame_buffer_size - bytes_done);
            bytes_done = data->frame_buffer_size;
        }

        /* not enough raw data, request more */
        if (rc == MPG123_NEED_MORE) {
            data->buffer_full = 0;
            continue;
        }

        break;
    } while (1);


    /* copy decoded frame to intermediate sample buffer, muxing channels
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
                memcpy(data->interleave_buffer + out_offset, data->frame_buffer + in_offset, sz);
            }
        }

        data->bytes_in_interleave_buffer += bytes_done;
    }
}

/**
 * Get frame size info for the current frame, needed by FSBs of varying padding.
 * Padding sometimes contains next frame header so we can't feed it to mpg123 or it gets confused.
 * Expected to be called at the beginning of a new frame.
 */
static int update_frame_sizes(mpeg_codec_data * data, STREAMFILE *streamfile, off_t offset) {
    if (!data->fixed_frame_size) {
        mpeg_frame_info info;

        /* Manually find new frame size. Not ideal but mpg123_info.framesize is wrong sometimes */
        if ( !mpeg_get_frame_info(streamfile, offset, &info) )
            goto fail;

        /* could mess some calcs */
        VGM_ASSERT(data->sample_rate_per_frame != info.sample_rate || data->samples_per_frame != info.frame_samples,
                "MPEG: variable frame info found @ 0x%08lx", offset);

        /* new frame */
        data->current_frame_size = info.frame_size;

        /* get FSB padding for MPEG1/2 Layer III (MPEG1 Layer II doesn't use it, and Layer I doesn't seem to be supported) */
        if (data->fsb_padding && info.layer == 3) {
            data->current_padding = (data->current_frame_size % data->fsb_padding) ?
                    data->fsb_padding - (data->current_frame_size % data->fsb_padding) : 0;
        }

        //VGM_LOG("off=%lx, ch=%i, sr=%i, spf=%i, fs=%x, v=%i, l=%i\n", offset, info.channels, info.sample_rate, info.frame_samples, info.frame_size, info.version, info.layer);
        //getchar();
    }

    return 1;

fail:
    return 0;
}

/**
 * Decode AHX mono frames.
 * mpg123 expects frames of 0x414 (160kbps, 22050Hz) but they actually vary and are much shorter
 */
void decode_fake_mpeg2_l2(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do) {
    int samples_done = 0;
    const int frame_size = AHX_EXPECTED_FRAME_SIZE;
    mpg123_handle *m = data->m;

    while (samples_done < samples_to_do) {
        size_t bytes_done;
        int rc;

        /* read more raw data */
        if (!data->buffer_full) {
            /* fill buffer up to next frame ending (or file ending) */
            int bytes_into_header = 0;
            const uint8_t header[4] = {0xff,0xf5,0xe0,0xc0};
            off_t frame_offset = 0;

            /* assume that we are starting at a header, skip it and look for the next one */
            read_streamfile(data->buffer, stream->offset+frame_offset, 4, stream->streamfile);
            frame_offset += 4;

            do {
                uint8_t byte;
                byte = read_8bit(stream->offset+frame_offset,stream->streamfile);
                data->buffer[frame_offset] = byte;
                frame_offset++;

                if (byte == header[bytes_into_header]) {
                    bytes_into_header++;
                } else {
                    /* This might have been the first byte of the header, so
                     * we need to check again.
                     * No need to get more complicated than this, though, since
                     * there are no repeated characters in the search string. */
                    if (bytes_into_header>0) {
                        frame_offset--;
                    }
                    bytes_into_header=0;
                }

                if (bytes_into_header==4) {
                    break;
                }
            } while (frame_offset < frame_size);

            if (bytes_into_header==4)
                frame_offset-=4;
            memset(data->buffer+frame_offset,0,frame_size-frame_offset);

            data->buffer_full = 1;
            data->buffer_used = 0;

            stream->offset += frame_offset;
        }

        /* feed new raw data to the decoder if needed, copy decodec results to output */
        if (!data->buffer_used) {
            rc = mpg123_decode(m,
                    data->buffer,frame_size,
                    (unsigned char *)(outbuf+samples_done),
                    (samples_to_do-samples_done)*sizeof(sample),
                    &bytes_done);
            data->buffer_used = 1;
        } else {
            rc = mpg123_decode(m,
                    NULL,0,
                    (unsigned char *)(outbuf+samples_done),
                    (samples_to_do-samples_done)*sizeof(sample),
                    &bytes_done);
        }

        /* not enough raw data, request more */
        if (rc == MPG123_NEED_MORE) {
            data->buffer_full = 0;
        }

        /* update copied samples */
        samples_done += bytes_done/sizeof(sample);/* mono */
    }
}

/*********/
/* UTILS */
/*********/

void free_mpeg(mpeg_codec_data *data) {
    if (!data)
        return;

    mpg123_delete(data->m);
    if (data->interleaved) {
        int i;
        for (i=0; i < data->ms_size; i++) {
            mpg123_delete(data->ms[i]);
        }
        free(data->ms);

        free(data->interleave_buffer);
        free(data->frame_buffer);
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

    /* input_offset is ignored as we can assume it will be 0 for a seek to sample 0 */
    mpg123_feedseek(data->m,0,SEEK_SET,&input_offset);

    /* reset multistream */ //todo check if stream offsets are properly reset
    if (data->interleaved) {
        int i;
        for (i=0; i < data->ms_size; i++) {
            mpg123_feedseek(data->ms[i],0,SEEK_SET,&input_offset);
        }

        data->bytes_in_interleave_buffer = 0;
        data->bytes_used_in_interleave_buffer = 0;
    }
}

void seek_mpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    /* won't work for fake AHX MPEG */
    off_t input_offset;
    mpeg_codec_data *data = vgmstream->codec_data;

    /* seek multistream */
    if (!data->interleaved) {
	    mpg123_feedseek(data->m, num_sample,SEEK_SET,&input_offset);
	    if (vgmstream->loop_ch)
	        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset + input_offset;
    } else {
        int i;
        /* re-start from 0 */
        for (i=0; i < data->ms_size; i++) {
            mpg123_feedseek(data->ms[i],0,SEEK_SET,&input_offset);
            if (vgmstream->loop_ch)
                vgmstream->loop_ch[i].offset = vgmstream->loop_ch[i].channel_start_offset;
        }
        /* manually add skip samples, since we don't really know the correct offset */
        data->samples_to_discard = num_sample;

        data->bytes_in_interleave_buffer = 0;
        data->bytes_used_in_interleave_buffer = 0;
    }

    data->buffer_full = 0;
    data->buffer_used = 0;
}


long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data *data) {
    struct mpg123_frameinfo mi;
    mpg123_handle *m = data->m;

    if (MPG123_OK != mpg123_info(m, &mi))
        return 0;

    /* In this case just return 0 and expect to fail (if used for num_samples)
     * We would need to read the number of frames in some frame header or count them to get samples */
    if (mi.vbr != MPG123_CBR) //maybe abr_rate could be used
        return 0;

    return (int64_t)bytes * mi.rate * 8 / (mi.bitrate * 1000);
}

/**
 * disables/enables stderr output, useful for MPEG known to contain recoverable errors
 */
void mpeg_set_error_logging(mpeg_codec_data * data, int enable) {
    mpg123_param(data->m, MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
    if (data->interleaved) {
        int i;
        for (i=0; i < data->ms_size; i++) {
            mpg123_param(data->ms[i], MPG123_ADD_FLAGS, MPG123_QUIET, !enable);
        }
    }
}

/*****************/
/* FRAME HELPERS */
/*****************/

/**
 * Gets info from a MPEG frame header at offset. Normally you would use mpg123_info but somehow
 * it's wrong at times (maybe because we use an ancient version) so here we do our thing.
 */
static int mpeg_get_frame_info(STREAMFILE *streamfile, off_t offset, mpeg_frame_info * info) {
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
    static const coding_t coding_types[3][3] = { /* [version][layer] */
            { coding_MPEG1_L1,  coding_MPEG1_L2,  coding_MPEG1_L3  },
            { coding_MPEG2_L1,  coding_MPEG2_L2,  coding_MPEG2_L3  },
            { coding_MPEG25_L1, coding_MPEG25_L2, coding_MPEG25_L3 },
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
    if (info->layer <= 0) goto fail;

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
        case 384:  info->frame_size = (12l  * info->bit_rate * 1000l / info->sample_rate + padding) * 4; break;
        case 576:  info->frame_size = (72l  * info->bit_rate * 1000l / info->sample_rate + padding); break;
        case 1152: info->frame_size = (144l * info->bit_rate * 1000l / info->sample_rate + padding); break;
    }
    //int fs2 = (info->frame_samples / 8 * info->bit_rate * 1000l) / info->sample_rate + padding;
    //VGM_LOG("fs=%x, fs2=%x, fsp=%i, br=%i\n", info->frame_size, fs2, info->frame_samples, info->bit_rate);
    //getchar();

    info->coding_type = coding_types[info->version-1][info->layer-1];

    return 1;

fail:
    return 0;
}

#endif
