#include "../vgmstream.h"

#ifdef VGM_USE_MPEG
#include <string.h>
#include <mpg123.h>
#include "coding.h"
#include "../util.h"

/* mono, mpg123 expects frames of 0x414 (160kbps, 22050Hz) but they
 * actually vary and are much shorter */
void decode_fake_mpeg2_l2(VGMSTREAMCHANNEL *stream,
        mpeg_codec_data * data,
        sample * outbuf, int32_t samples_to_do) {
    int samples_done = 0;

    while (samples_done < samples_to_do) {
        size_t bytes_done;
        int rc;

        if (!data->buffer_full) {
            /* fill buffer up to next frame ending (or file ending) */
            int bytes_into_header = 0;
            const uint8_t header[4] = {0xff,0xf5,0xe0,0xc0};
            off_t frame_offset = 0;

            /* assume that we are starting at a header, skip it and look for the
             * next one */
            read_streamfile(data->buffer, stream->offset+frame_offset, 4,
                    stream->streamfile);
            frame_offset += 4;

            do {
                uint8_t byte;
                byte =
                    read_8bit(stream->offset+frame_offset,stream->streamfile);
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
            } while (frame_offset < AHX_EXPECTED_FRAME_SIZE);

            if (bytes_into_header==4) frame_offset-=4;
            memset(data->buffer+frame_offset,0,
                    AHX_EXPECTED_FRAME_SIZE-frame_offset);

            data->buffer_full = 1;
            data->buffer_used = 0;

            stream->offset += frame_offset;
        }

        if (!data->buffer_used) {
            rc = mpg123_decode(data->m,
                    data->buffer,AHX_EXPECTED_FRAME_SIZE,
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

        if (rc == MPG123_NEED_MORE) data->buffer_full = 0;

        samples_done += bytes_done/sizeof(sample);
    }
}

mpeg_codec_data *init_mpeg_codec_data(STREAMFILE *streamfile, off_t start_offset, long given_sample_rate, int given_channels, coding_t *coding_type, int * actual_sample_rate, int * actual_channels) {
    int rc;
    off_t read_offset;
    mpeg_codec_data *data = NULL;

    data = calloc(1,sizeof(mpeg_codec_data));
    if (!data) goto mpeg_fail;

    data->m = mpg123_new(NULL,&rc);
    if (rc==MPG123_NOT_INITIALIZED) {
        if (mpg123_init()!=MPG123_OK) goto mpeg_fail;
        data->m = mpg123_new(NULL,&rc);
        if (rc!=MPG123_OK) goto mpeg_fail;
    } else if (rc!=MPG123_OK) {
        goto mpeg_fail;
    }

    mpg123_param(data->m,MPG123_REMOVE_FLAGS,MPG123_GAPLESS,0.0);

    if (mpg123_open_feed(data->m)!=MPG123_OK) {
        goto mpeg_fail;
    }

    /* check format */
    read_offset=0;
    do {
        size_t bytes_done;
        if (read_streamfile(data->buffer, start_offset+read_offset,
                    MPEG_BUFFER_SIZE,streamfile) !=
                MPEG_BUFFER_SIZE) goto mpeg_fail;
        read_offset+=1;
        rc = mpg123_decode(data->m,data->buffer,MPEG_BUFFER_SIZE,
                NULL,0,&bytes_done);
        if (rc != MPG123_OK && rc != MPG123_NEW_FORMAT &&
                rc != MPG123_NEED_MORE) goto mpeg_fail;
    } while (rc != MPG123_NEW_FORMAT);

    {
        long rate;
        int channels,encoding;
        struct mpg123_frameinfo mi;
        rc = mpg123_getformat(data->m,&rate,&channels,&encoding);
        if (rc != MPG123_OK) goto mpeg_fail;
        //fprintf(stderr,"getformat ok, sr=%ld (%ld) ch=%d (%d) enc=%d (%d)\n",rate,given_sample_rate,channels,vgmstream->channels,encoding,MPG123_ENC_SIGNED_16);
        if ((given_sample_rate != -1 && rate != given_sample_rate) ||
            (given_channels != -1 && channels != given_channels) ||
            encoding != MPG123_ENC_SIGNED_16) goto mpeg_fail;
        mpg123_info(data->m,&mi);
        if (given_sample_rate != -1 &&
            mi.rate != given_sample_rate) goto mpeg_fail;

        //fprintf(stderr,"mi.version=%d, mi.layer=%d\n",mi.version,mi.layer);

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
        else goto mpeg_fail;

		if ( actual_sample_rate ) *actual_sample_rate = rate;
		if ( actual_channels ) *actual_channels = channels;
    }

    /* reinit, to ignore the reading we've done so far */
    mpg123_open_feed(data->m);

    return data;

mpeg_fail:
    fprintf(stderr, "mpeg_fail start_offset=%x\n",(unsigned int)start_offset);
    if (data) {
        mpg123_delete(data->m);
        free(data);
    }
    return NULL;
}

/* decode anything mpg123 can */
void decode_mpeg(VGMSTREAMCHANNEL *stream,
        mpeg_codec_data * data,
        sample * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;

    while (samples_done < samples_to_do) {
        size_t bytes_done;
        int rc;

        if (!data->buffer_full) {
            data->bytes_in_buffer = read_streamfile(data->buffer,
                    stream->offset,MPEG_BUFFER_SIZE,stream->streamfile);

			if (!data->bytes_in_buffer) {
				memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample));
				break;
			}

            data->buffer_full = 1;
            data->buffer_used = 0;

            stream->offset += data->bytes_in_buffer;
        }

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

        if (rc == MPG123_NEED_MORE) data->buffer_full = 0;

        samples_done += bytes_done/sizeof(sample)/channels;
    }
}

long mpeg_bytes_to_samples(long bytes, const struct mpg123_frameinfo *mi) {
    return (int64_t)bytes * mi->rate * 8 / (mi->bitrate * 1000);
}

#endif
