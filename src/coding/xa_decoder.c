#include "coding.h"
#include "../util.h"

const int SH = 4;
const int SHC = 10;

double K0[4] = { 0.0, 0.9375, 1.796875,  1.53125};
double K1[4] = { 0.0,    0.0,  -0.8125,-0.859375};

int IK0(int fid)
{ return ((int)((-K0[fid]) * (1 << SHC))); }

int IK1(int fid)
{ return ((int)((-K1[fid]) * (1 << SHC))); }

int CLAMP(int value, int Minim, int Maxim)
{
    if (value < Minim) value = Minim;
    if (value > Maxim) value = Maxim;
    return value;
}

static uint8_t get_high_nibble=1;

void decode_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {

	int predict_nr, shift_factor, sample;
	int32_t hist1=stream->adpcm_history1_32;
	int32_t hist2=stream->adpcm_history2_32;
	int HeadTable[8]={0,2,8,10};

	short scale;
	int i;
	int32_t sample_count;

	int framesin = first_sample / 28;

	get_high_nibble=!get_high_nibble;

	predict_nr = read_8bit(stream->offset+HeadTable[framesin]+get_high_nibble,stream->streamfile) >> 4;
	shift_factor = read_8bit(stream->offset+HeadTable[framesin]+get_high_nibble,stream->streamfile) & 0xf;
	
	first_sample = first_sample % 28;
	
	for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        short sample_byte = (short)read_8bit(stream->offset+16+framesin+(i*4),stream->streamfile);

		scale = ((get_high_nibble ?
			     sample_byte >> 4 :
				 sample_byte & 0x0f)<<12);

        sample = (short)(scale & 0xf000) >> shift_factor; 
		sample <<= SH;
        sample -= (IK0(predict_nr) * hist1 + (IK1(predict_nr) * hist2)) >> SHC; 

		hist2=hist1;
		hist1=sample;

		sample = CLAMP(sample, -32768 << SH, 32767 << SH);
		outbuf[sample_count] = (short)(sample >> SH);
	}

	stream->adpcm_history1_32=hist1;
	stream->adpcm_history2_32=hist2;
}
