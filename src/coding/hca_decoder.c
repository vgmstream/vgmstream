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

    data->sample_ptr = clHCA_samplesPerBlock;

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
	int32_t samples_remain = clHCA_samplesPerBlock - data->sample_ptr;

    //todo improve (can't be done on init since data->info is only read after setting key)
    if (!data->data_buffer) {
        data->data_buffer = malloc(data->info.blockSize);
        if (!data->data_buffer) return;

        data->sample_buffer = malloc(sizeof(signed short) * data->info.channelCount * clHCA_samplesPerBlock);
        if (!data->sample_buffer) return;
    }


	if ( data->samples_discard ) {
		if ( samples_remain <= data->samples_discard ) {
			data->samples_discard -= samples_remain;
			samples_remain = 0;
		}
		else {
			samples_remain -= data->samples_discard;
			data->sample_ptr += data->samples_discard;
			data->samples_discard = 0;
		}
	}

	if (samples_remain > samples_to_do)
	    samples_remain = samples_to_do;
    memcpy(outbuf, data->sample_buffer + data->sample_ptr * data->info.channelCount, samples_remain * data->info.channelCount * sizeof(sample));
    outbuf += samples_remain * data->info.channelCount;
    data->sample_ptr += samples_remain;
    samples_done += samples_remain;

    
    /* feed */
	while ( samples_done < samples_to_do ) {
        const unsigned int blockSize = data->info.blockSize;
        const unsigned int channelCount = data->info.channelCount;
        const unsigned int address = data->info.dataOffset + data->curblock * blockSize;
        int status;
        size_t bytes;

        /* EOF/error */
        if (data->curblock >= data->info.blockCount) {
            memset(outbuf, 0, (samples_to_do - samples_done) * channelCount * sizeof(sample));
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
        data->curblock++;

        /* extract samples */
        clHCA_DecodeSamples16(data->handle, data->sample_buffer);
        
		samples_remain = clHCA_samplesPerBlock;
		data->sample_ptr = 0;
		if ( data->samples_discard ) {
			if ( samples_remain <= data->samples_discard ) {
				data->samples_discard -= samples_remain;
				samples_remain = 0;
			}
			else {
				samples_remain -= data->samples_discard;
				data->sample_ptr = data->samples_discard;
				data->samples_discard = 0;
			}
		}

        if (samples_remain > samples_to_do - samples_done)
            samples_remain = samples_to_do - samples_done;
        memcpy(outbuf, data->sample_buffer, samples_remain * channelCount * sizeof(sample));
		samples_done += samples_remain;
		outbuf += samples_remain * channelCount;
		data->sample_ptr = samples_remain;
	}
}


void reset_hca(VGMSTREAM *vgmstream) {
    hca_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    data->curblock = 0;
    data->sample_ptr = clHCA_samplesPerBlock;
    data->samples_discard = 0;
}

void loop_hca(VGMSTREAM *vgmstream) {
    hca_codec_data *data = (hca_codec_data *)(vgmstream->codec_data);
    if (!data) return;

    data->curblock = data->info.loopStart;
    data->sample_ptr = clHCA_samplesPerBlock;
    data->samples_discard = 0;
}

void free_hca(hca_codec_data * data) {
    if (data) {
        close_streamfile(data->streamfile);
        clHCA_done(data->handle);
        free(data->handle);
        free(data->data_buffer);
        free(data->sample_buffer);
        free(data);
    }
}
