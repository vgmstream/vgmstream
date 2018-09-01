#include "coding.h"


/* init a HCA stream; STREAMFILE will be duplicated for internal use. */
hca_codec_data * init_hca(STREAMFILE *streamFile) {
    char filename[PATH_LIMIT];
    uint8_t header_buffer[0x2000]; /* hca header buffer data (probable max ~0x400) */
    hca_codec_data * data = NULL; /* vgmstream HCA context */
    int header_size;
    int status;

    /* test header */
    if (read_streamfile(header_buffer, 0x00, 0x08, streamFile) != 0x08)
        goto fail;
    header_size = clHCA_isOurFile(header_buffer, 0x08);
    if (header_size < 0 || header_size > 0x1000)
        goto fail;
    if (read_streamfile(header_buffer, 0x00, header_size, streamFile) != header_size)
        goto fail;

    /* init vgmstream context */
    data = calloc(1, sizeof(hca_codec_data));
    if (!data) goto fail;

    /* init library handle */
    data->handle = calloc(1, clHCA_sizeof());
    clHCA_clear(data->handle);

    status = clHCA_DecodeHeader(data->handle, header_buffer, header_size); /* parse header */
    if (status < 0) goto fail;

    status = clHCA_getInfo(data->handle, &data->info); /* extract header info */
    if (status < 0) goto fail;

    data->data_buffer = malloc(data->info.blockSize);
    if (!data->data_buffer) goto fail;

    data->sample_buffer = malloc(sizeof(signed short) * data->info.channelCount * data->info.samplesPerBlock);
    if (!data->sample_buffer) goto fail;

    /* load streamfile for reads */
    get_streamfile_name(streamFile,filename, sizeof(filename));
    data->streamfile = open_streamfile(streamFile,filename);
    if (!data->streamfile) goto fail;

    /* set initial values */
    reset_hca(data);

    return data;

fail:
    free_hca(data);
    return NULL;
}

void decode_hca(hca_codec_data * data, sample * outbuf, int32_t samples_to_do) {
	int samples_done = 0;
    const unsigned int channels = data->info.channelCount;
    const unsigned int blockSize = data->info.blockSize;


    while (samples_done < samples_to_do) {

        if (data->samples_filled) {
            int samples_to_get = data->samples_filled;

            if (data->samples_to_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_to_discard)
                    samples_to_get = data->samples_to_discard;
                data->samples_to_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do - samples_done)
                    samples_to_get = samples_to_do - samples_done;

                memcpy(outbuf + samples_done*channels,
                       data->sample_buffer + data->samples_consumed*channels,
                       samples_to_get*channels * sizeof(sample));
                samples_done += samples_to_get;
            }

            /* mark consumed samples */
            data->samples_consumed += samples_to_get;
            data->samples_filled -= samples_to_get;
        }
        else {
            off_t offset = data->info.headerSize + data->current_block * blockSize;
            int status;
            size_t bytes;

            /* EOF/error */
            if (data->current_block >= data->info.blockCount) {
                memset(outbuf, 0, (samples_to_do - samples_done) * channels * sizeof(sample));
                break;
            }

            /* read frame */
            bytes = read_streamfile(data->data_buffer, offset, blockSize, data->streamfile);
            if (bytes != blockSize) {
                VGM_LOG("HCA: read %x vs expected %x bytes at %lx\n", bytes, blockSize, offset);
                break;
            }

            /* decode frame */
            status = clHCA_DecodeBlock(data->handle, (void*)(data->data_buffer), blockSize);
            if (status < 0) {
                VGM_LOG("HCA: decode fail at %lx", offset);
                break;
            }

            /* extract samples */
            clHCA_ReadSamples16(data->handle, data->sample_buffer);

            data->current_block++;
            data->samples_consumed = 0;
            data->samples_filled += data->info.samplesPerBlock;
        }
    }
}


void reset_hca(hca_codec_data * data) {
    if (!data) return;

    data->current_block = 0;
    data->samples_filled = 0;
    data->samples_consumed = 0;
    data->samples_to_discard = data->info.encoderDelay;
}

void loop_hca(hca_codec_data * data) {
    if (!data) return;

    data->current_block = data->info.loopStartBlock;
    data->samples_filled = 0;
    data->samples_consumed = 0;
    data->samples_to_discard = data->info.loopStartDelay;
}

void free_hca(hca_codec_data * data) {
    if (!data) return;

    close_streamfile(data->streamfile);
    clHCA_done(data->handle);
    free(data->handle);
    free(data->data_buffer);
    free(data->sample_buffer);
    free(data);
}
