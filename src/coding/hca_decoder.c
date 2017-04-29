#include "coding.h"

void decode_hca(hca_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
	int samples_done = 0;

	int32_t samples_remain = clHCA_samplesPerBlock - data->sample_ptr;
    
    void *hca_data = NULL;
    
    clHCA *hca;

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

	if ( samples_remain > samples_to_do ) samples_remain = samples_to_do;

    memcpy( outbuf, data->sample_buffer + data->sample_ptr * data->info.channelCount, samples_remain * data->info.channelCount * sizeof(sample) );
    
    outbuf += samples_remain * data->info.channelCount;
    
    data->sample_ptr += samples_remain;
    
    samples_done += samples_remain;
    
    hca_data = malloc( data->info.blockSize );
    
    if ( !hca_data ) return;
    
    hca = (clHCA *)(data + 1);

	while ( samples_done < samples_to_do ) {
        const unsigned int blockSize = data->info.blockSize;
        const unsigned int channelCount = data->info.channelCount;
        const unsigned int address = data->info.dataOffset + data->curblock * blockSize;
        
        if (data->curblock >= data->info.blockCount) {
            memset(outbuf, 0, (samples_to_do - samples_done) * channelCount * sizeof(sample));
            break;
        }
        
        if ( read_streamfile((uint8_t*) hca_data, data->start + address, blockSize, data->streamfile) != blockSize )
            break;
        
        if ( clHCA_Decode( hca, hca_data, blockSize, address ) < 0 )
            break;
        
        ++data->curblock;

        clHCA_DecodeSamples16( hca, data->sample_buffer );
        
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

        if ( samples_remain > samples_to_do - samples_done ) samples_remain = samples_to_do - samples_done;
        memcpy( outbuf, data->sample_buffer, samples_remain * channelCount * sizeof(sample) );
		samples_done += samples_remain;
		outbuf += samples_remain * channelCount;
		data->sample_ptr = samples_remain;
	}
    
    free( hca_data );
}


void reset_hca(VGMSTREAM *vgmstream) {
    hca_codec_data *data = vgmstream->codec_data;
    /*clHCA *hca = (clHCA *)(data + 1);*/
    data->curblock = 0;
    data->sample_ptr = clHCA_samplesPerBlock;
    data->samples_discard = 0;
}

void loop_hca(VGMSTREAM *vgmstream) {
    hca_codec_data *data = (hca_codec_data *)(vgmstream->codec_data);
    data->curblock = data->info.loopStart;
    data->sample_ptr = clHCA_samplesPerBlock;
    data->samples_discard = 0;
}

void free_hca(hca_codec_data * data) {
    if (data) {
        clHCA *hca = (clHCA *)(data + 1);
        clHCA_done(hca);
        if (data->streamfile)
            close_streamfile(data->streamfile);
        free(data);
    }
}
