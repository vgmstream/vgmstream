#include "coding.h"
#include "../util.h"
#include "../vgmstream.h"

#ifdef VGM_USE_MPEG
#include <mpg123.h>

#define AHX_EXPECTED_FRAME_SIZE 0x414
#define MPEG_DEFAULT_BUFFER_SIZE 0x1000  /* should be >= AHX_EXPECTED_FRAME_SIZE */


/**
 * Inits regular MPEG, updating pointers if passed and validating channels/sample rate when not -1.
 */
mpeg_codec_data *init_mpeg_codec_data(STREAMFILE *streamfile, off_t start_offset, long given_sample_rate, int given_channels, coding_t *coding_type, int * actual_sample_rate, int * actual_channels) {
    mpeg_codec_data *data = NULL;
    int rc;
    off_t read_offset;


    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;

    data->m = mpg123_new(NULL,&rc);
    if (rc == MPG123_NOT_INITIALIZED) {
        if (mpg123_init() != MPG123_OK) goto fail;

        data->m = mpg123_new(NULL,&rc);
        if (rc != MPG123_OK) goto fail;
    } else if (rc != MPG123_OK) {
        goto fail;
    }

    mpg123_param(data->m,MPG123_REMOVE_FLAGS,MPG123_GAPLESS,0.0);

    if (mpg123_open_feed(data->m) != MPG123_OK) {
        goto fail;
    }

    /* check format */
    read_offset=0;
    do {
        size_t bytes_done;
        if (read_streamfile(data->buffer, start_offset+read_offset, data->buffer_size, streamfile) != data->buffer_size)
            goto fail;
        read_offset+=1;

        rc = mpg123_decode(data->m, data->buffer,data->buffer_size, NULL,0, &bytes_done);
        if (rc != MPG123_OK && rc != MPG123_NEW_FORMAT && rc != MPG123_NEED_MORE) goto fail;
    } while (rc != MPG123_NEW_FORMAT);

    {
        long rate;
        int channels,encoding;
        struct mpg123_frameinfo mi;

        rc = mpg123_getformat(data->m,&rate,&channels,&encoding);
        if (rc != MPG123_OK) goto fail;

        if ((given_sample_rate != -1 && rate != given_sample_rate)
                || (given_channels != -1 && channels != given_channels)
                || encoding != MPG123_ENC_SIGNED_16)
            goto fail;

        mpg123_info(data->m,&mi);
        if (given_sample_rate != -1 && mi.rate != given_sample_rate)
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

		if ( actual_sample_rate ) *actual_sample_rate = rate;
		if ( actual_channels ) *actual_channels = channels;
    }

    /* reinit, to ignore the reading we've done so far */
    mpg123_open_feed(data->m);

    return data;

fail:
    free_mpeg(data);
    return NULL;
}

/**
 * Inits MPEG for AHX, which ignores channels and sample rate in the MPEG headers.
 */
mpeg_codec_data *init_mpeg_codec_data_ahx(STREAMFILE *streamFile, off_t start_offset, int channel_count) {
    mpeg_codec_data *data = NULL;
    int rc;


    /* init codec */
    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto fail;

    data->buffer_size = MPEG_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;

    data->m = mpg123_new(NULL,&rc);
    if (rc == MPG123_NOT_INITIALIZED) {
        if (mpg123_init() != MPG123_OK) goto fail;

        data->m = mpg123_new(NULL,&rc);
        if (rc != MPG123_OK) goto fail;
    } else if (rc!=MPG123_OK) {
        goto fail;
    }

    if (mpg123_open_feed(data->m) != MPG123_OK) {
        goto fail;
    }

    return data;

fail:
    free_mpeg(data);
    return NULL;
}


/**
 * Decode anything mpg123 can.
 *
 * Feeds raw data and extracts decoded samples as needed.
 */
void decode_mpeg(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;

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

        /* feed new raw data to the decoder if needed, copy decodec results to output */
        if (!data->buffer_used) {
            rc = mpg123_decode(data->m,
                    data->buffer,data->bytes_in_buffer,
                    (unsigned char *)(outbuf+samples_done*channels),
                    (samples_to_do-samples_done)*sizeof(sample)*channels,
                    &bytes_done);
            data->buffer_used = 1;
        } else {
            rc = mpg123_decode(data->m,
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
 * Decode AHX mono frames.
 *
 * mpg123 expects frames of 0x414 (160kbps, 22050Hz) but they actually vary and are much shorter
 */
void decode_fake_mpeg2_l2(VGMSTREAMCHANNEL *stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do) {
    int samples_done = 0;
    const int frame_size = AHX_EXPECTED_FRAME_SIZE;

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
            rc = mpg123_decode(data->m,
                    data->buffer,frame_size,
                    (unsigned char *)(outbuf+samples_done),
                    (samples_to_do-samples_done)*sizeof(sample),
                    &bytes_done);
            data->buffer_used = 1;
        } else {
            rc = mpg123_decode(data->m,
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


void free_mpeg(mpeg_codec_data *data) {
    if (!data)
        return;

    mpg123_delete(data->m);
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
    data->buffer_full = data->buffer_used = 0;
}

void seek_mpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    /* won't work for fake MPEG */
    off_t input_offset;
    mpeg_codec_data *data = vgmstream->codec_data;

    mpg123_feedseek(data->m, num_sample,SEEK_SET,&input_offset);
    vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset + input_offset;
    data->buffer_full = 0;
    data->buffer_used = 0;
}


long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data *data) {
    struct mpg123_frameinfo mi;

    if (MPG123_OK != mpg123_info(data->m, &mi))
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
}
#endif
