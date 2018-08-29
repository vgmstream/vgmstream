#include "coding.h"


/* init a HCA stream; STREAMFILE will be duplicated for internal use. */
hca_codec_data * init_hca(STREAMFILE *streamFile) {
    char filename[PATH_LIMIT];
    hca_codec_data * data = NULL; /* vgmstream HCA context */

    /* init vgmstream context */
    data = calloc(1, sizeof(hca_codec_data));
    if (!data) goto fail;

    /* init library handle */
    data->handle = calloc(1, clHCA_sizeof());

    /* load streamfile for reads */
    get_streamfile_name(streamFile,filename, sizeof(filename));
    data->streamfile = open_streamfile(streamFile,filename);
    if (!data->streamfile) goto fail;

    return data;

fail:
    free_hca(data);
    return NULL;
}

void decode_hca(hca_codec_data * data, sample * outbuf, int32_t samples_to_do) {
	int samples_done = 0;
    const unsigned int channels = data->info.channelCount;
    const unsigned int blockSize = data->info.blockSize;


    //todo improve (can't be done on init since data->info is only read after setting key)
    if (!data->data_buffer) {
        data->data_buffer = malloc(data->info.blockSize);
        if (!data->data_buffer) return;

        data->sample_buffer = malloc(sizeof(signed short) * data->info.channelCount * clHCA_samplesPerBlock);
        if (!data->sample_buffer) return;
    }


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
            const unsigned int address = data->info.dataOffset + data->current_block * blockSize;
            int status;
            size_t bytes;

            /* EOF/error */
            if (data->current_block >= data->info.blockCount) {
                memset(outbuf, 0, (samples_to_do - samples_done) * channels * sizeof(sample));
                break;
            }

            /* read frame */
            bytes = read_streamfile(data->data_buffer, address, blockSize, data->streamfile);
            if (bytes != blockSize) {
                VGM_LOG("HCA: read %x vs expected %x bytes at %x\n", bytes, blockSize, address);
                break;
            }

            /* decode frame */
            status = clHCA_Decode(data->handle, (void*)(data->data_buffer), blockSize, address);
            if (status < 0) {
                VGM_LOG("HCA: decode fail at %x", address);
                break;
            }

            /* extract samples */
            clHCA_DecodeSamples16(data->handle, data->sample_buffer);

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
