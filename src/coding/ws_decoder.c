#include <math.h>
#include "coding.h"
#include "../util.h"

/* Westwood Studios ADPCM */

/* Based on Valery V. Ansimovsky's WS-AUD.txt */

void decode_ws(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {

	int32_t hist = stream->adpcm_history1_32;

	int i;
	int32_t sample_count;
	
	for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
		outbuf[sample_count] = 0;
	}
	stream->adpcm_history1_32=hist;
}
