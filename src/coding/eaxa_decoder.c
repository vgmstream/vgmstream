#include "coding.h"
#include "../util.h"

long EA_XA_TABLE[28] = {0,0,240,0,460,-208,0x0188,-220,
					      0x0000,0x0000,0x00F0,0x0000,
					      0x01CC,0x0000,0x0188,0x0000,
					      0x0000,0x0000,0x0000,0x0000,
					                  -208,-1,-220,-1,
					      0x0000,0x0000,0x0000,0x3F70};

void decode_eaxa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    uint8_t frame_info;
    int32_t sample_count;
	long coef1,coef2;
	int i,shift;
	off_t channel_offset=stream->channel_start_offset;

	first_sample = first_sample%28;
	frame_info = (uint8_t)read_8bit(stream->offset+channel_offset,stream->streamfile);

	if(frame_info==0xEE) {

		channel_offset++;
		stream->adpcm_history1_32 = read_16bitBE(stream->offset+channel_offset+(2*channel),stream->streamfile);
		stream->adpcm_history2_32 = read_16bitBE(stream->offset+channel_offset+2+(2*channel),stream->streamfile);

		channel_offset+=(2*channelspacing);

		for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
			outbuf[sample_count]=read_16bitBE(stream->offset+channel_offset,stream->streamfile);
			channel_offset+=2;
		}

		// Only increment offset on complete frame
		if(channel_offset-stream->channel_start_offset==(2*28)+5)
			stream->channel_start_offset+=(2*28)+5;

	} else {
	
		
		coef1 = EA_XA_TABLE[((frame_info >> 4) & 0x0F) << 1];
		coef2 = EA_XA_TABLE[(((frame_info >> 4) & 0x0F) << 1) + 1];
		shift = (frame_info & 0x0F) + 8;
		
		channel_offset++;

		for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
			uint8_t sample_byte = (uint8_t)read_8bit(stream->offset+channel_offset+i/2,stream->streamfile);
			int32_t sample = ((((i&1?
						    sample_byte & 0x0F:
							sample_byte >> 4
						  ) << 0x1C) >> shift) +
						  (coef1 * stream->adpcm_history1_32) + (coef2 * stream->adpcm_history2_32)) >> 8;
			
			outbuf[sample_count] = clamp16(sample);
			stream->adpcm_history2_32 = stream->adpcm_history1_32;
			stream->adpcm_history1_32 = sample;
		}
		
		channel_offset+=i/2;

		// Only increment offset on complete frame
		if(channel_offset-stream->channel_start_offset==0x0F)
			stream->channel_start_offset+=0x0F;
	}
}
