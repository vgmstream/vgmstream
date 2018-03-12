#include "../vgmstream.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
static void convert_samples(INT_PCM * src, sample * dest, int32_t count) {
	int32_t i;
	for ( i = 0; i < count; i++ ) {
		INT_PCM sample = *src++;
		sample >>= SAMPLE_BITS - 16;
		if ( ( sample + 0x8000 ) & 0xFFFF0000 ) sample = 0x7FFF ^ ( sample >> 31 );
		*dest++ = sample;
	}
}

void decode_mp4_aac(mp4_aac_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
	int samples_done = 0;

	uint8_t * buffer = NULL;
	uint32_t buffer_size;
	UINT ubuffer_size, bytes_valid;

	CStreamInfo * stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );

	int32_t samples_remain = data->samples_per_frame - data->sample_ptr;

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

	convert_samples( data->sample_buffer + data->sample_ptr * stream_info->numChannels, outbuf, samples_remain * stream_info->numChannels );

	outbuf += samples_remain * stream_info->numChannels;

	data->sample_ptr += samples_remain;

	samples_done += samples_remain;

	while ( samples_done < samples_to_do ) {
		if (data->sampleId >= data->numSamples) {
			memset(outbuf, 0, (samples_to_do - samples_done) * stream_info->numChannels * sizeof(sample));
			break;
		}
		if (!MP4ReadSample( data->h_mp4file, data->track_id, ++data->sampleId, (uint8_t**)(&buffer), (uint32_t*)(&buffer_size), 0, 0, 0, 0)) return;
		ubuffer_size = buffer_size;
		bytes_valid = buffer_size;
		if ( aacDecoder_Fill( data->h_aacdecoder, &buffer, &ubuffer_size, &bytes_valid ) || bytes_valid ) {
			free( buffer );
			return;
		}
		if ( aacDecoder_DecodeFrame( data->h_aacdecoder, data->sample_buffer, ( (6) * (2048)*4 ), 0 ) ) {
			free( buffer );
			return;
		}
		free( buffer ); buffer = NULL;
		stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );
		samples_remain = data->samples_per_frame = stream_info->frameSize;
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
		convert_samples( data->sample_buffer + data->sample_ptr * stream_info->numChannels, outbuf, samples_remain * stream_info->numChannels );
		samples_done += samples_remain;
		outbuf += samples_remain * stream_info->numChannels;
		data->sample_ptr = samples_remain;
	}
}


void reset_mp4_aac(VGMSTREAM *vgmstream) {
    mp4_aac_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    data->sampleId = 0;
    data->sample_ptr = data->samples_per_frame;
    data->samples_discard = 0;
}

void seek_mp4_aac(VGMSTREAM *vgmstream, int32_t num_sample) {
    mp4_aac_codec_data *data = (mp4_aac_codec_data *)(vgmstream->codec_data);
    if (!data) return;

    data->sampleId = 0;
    data->sample_ptr = data->samples_per_frame;
    data->samples_discard = num_sample;
}

void free_mp4_aac(mp4_aac_codec_data * data) {
    if (data) {
        if (data->h_aacdecoder) aacDecoder_Close(data->h_aacdecoder);
        if (data->h_mp4file) MP4Close(data->h_mp4file, 0);
        if (data->if_file.streamfile) close_streamfile(data->if_file.streamfile);
        free(data);
    }
}

#endif
