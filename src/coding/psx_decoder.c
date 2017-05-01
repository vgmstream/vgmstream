#include <math.h>
#include "coding.h"
#include "../util.h"

/* for some algos, maybe closer to the real thing */
#define VAG_USE_INTEGER_TABLE   0

/* PS ADPCM table (precalculated divs) */
static const double VAG_f[5][2] = {
        {   0.0        ,   0.0        },
        {  60.0 / 64.0 ,   0.0        },
        { 115.0 / 64.0 , -52.0 / 64.0 },
        {  98.0 / 64.0 , -55.0 / 64.0 },
        { 122.0 / 64.0 , -60.0 / 64.0 }
};
#if VAG_USE_INTEGER_TABLE
/* PS ADPCM table */
static const int8_t VAG_coefs[5][2] = {
        {   0 ,   0 },
        {  60 ,   0 },
        { 115 , -52 },
        {  98 , -55 },
        { 122 , -60 }
};
#endif


/**
 * Sony's PS ADPCM (sometimes called VAG), decodes 16 bytes into 28 samples.
 * The first 2 bytes are a header (shift, predictor, optional flag).
 * All variants are the same with minor differences.
 *
 * Flags:
 *  0x0: Nothing
 *  0x1: End marker + decode
 *  0x2: Loop region
 *  0x3: Loop end
 *  0x4: Start marker
 *  0x5: ?
 *  0x6: Loop start
 *  0x7: End marker + don't decode
 *  0x8+ Not valid
 */

/* default */
void decode_psx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {

	int predict_nr, shift_factor, sample;
	int32_t hist1=stream->adpcm_history1_32;
	int32_t hist2=stream->adpcm_history2_32;

	short scale;
	int i;
	int32_t sample_count;
	uint8_t flag;

	int framesin = first_sample/28;

	predict_nr = read_8bit(stream->offset+framesin*16,stream->streamfile) >> 4;
	shift_factor = read_8bit(stream->offset+framesin*16,stream->streamfile) & 0xf;
	flag = read_8bit(stream->offset+framesin*16+1,stream->streamfile); /* only lower nibble needed */

	first_sample = first_sample % 28;
	
	for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {

		sample=0;

		if(flag<0x07) {
		
			short sample_byte = (short)read_8bit(stream->offset+(framesin*16)+2+i/2,stream->streamfile);

			scale = ((i&1 ? /* odd/even byte */
				     sample_byte >> 4 :
					 sample_byte & 0x0f)<<12);

			sample=(int)((scale >> shift_factor)+hist1*VAG_f[predict_nr][0]+hist2*VAG_f[predict_nr][1]);
		}

		outbuf[sample_count] = clamp16(sample);
		hist2=hist1;
		hist1=sample;
	}
	stream->adpcm_history1_32=hist1;
	stream->adpcm_history2_32=hist2;
}

/* encrypted */
void decode_psx_bmdx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {

	int predict_nr, shift_factor, sample;
	int32_t hist1=stream->adpcm_history1_32;
	int32_t hist2=stream->adpcm_history2_32;

	short scale;
	int i;
	int32_t sample_count;
	uint8_t flag;

	int framesin = first_sample/28;
    int head = read_8bit(stream->offset+framesin*16,stream->streamfile) ^ stream->bmdx_xor;

	predict_nr = ((head >> 4) & 0xf);
	shift_factor = (head & 0xf);
	flag = read_8bit(stream->offset+framesin*16+1,stream->streamfile);

	first_sample = first_sample % 28;
	
	for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {

		sample=0;

		if(flag<0x07) {
		
			short sample_byte = (short)read_8bit(stream->offset+(framesin*16)+2+i/2,stream->streamfile);
            if (i/2 == 0)
                sample_byte = (short)(int8_t)(sample_byte+stream->bmdx_add);

			scale = ((i&1 ?
				     sample_byte >> 4 :
					 sample_byte & 0x0f)<<12);

			sample=(int)((scale >> shift_factor)+hist1*VAG_f[predict_nr][0]+hist2*VAG_f[predict_nr][1]);
		}

		outbuf[sample_count] = clamp16(sample);
		hist2=hist1;
		hist1=sample;
	}
	stream->adpcm_history1_32=hist1;
	stream->adpcm_history2_32=hist2;
}

/* some games have garbage (?) in their flags, this decoder just ignores that byte */
void decode_psx_badflags(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {

	int predict_nr, shift_factor, sample;
	int32_t hist1=stream->adpcm_history1_32;
	int32_t hist2=stream->adpcm_history2_32;

	short scale;
	int i;
	int32_t sample_count;

	int framesin = first_sample/28;

	predict_nr = read_8bit(stream->offset+framesin*16,stream->streamfile) >> 4;
	shift_factor = read_8bit(stream->offset+framesin*16,stream->streamfile) & 0xf;
	first_sample = first_sample % 28;
	
	for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        short sample_byte = (short)read_8bit(stream->offset+(framesin*16)+2+i/2,stream->streamfile);

        scale = ((i&1 ?
                    sample_byte >> 4 :
                    sample_byte & 0x0f)<<12);

        sample=(int)((scale >> shift_factor)+hist1*VAG_f[predict_nr][0]+hist2*VAG_f[predict_nr][1]);

		outbuf[sample_count] = clamp16(sample);
		hist2=hist1;
		hist1=sample;
	}
	stream->adpcm_history1_32=hist1;
	stream->adpcm_history2_32=hist2;
}


/* configurable frame size, with no flag
 * Found in PS3 Afrika (SGXD type 5) in size 4, FF XI in sizes 3/5/9/41, Blur and James Bond in size 33. */
void decode_psx_configurable(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size) {
    uint8_t predict_nr, shift, byte;
    int16_t scale = 0;

    int32_t sample;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    int i, sample_count, bytes_per_frame, samples_per_frame;
    const int header_size = 1;
	int framesin;

    bytes_per_frame = frame_size - header_size;
    samples_per_frame = bytes_per_frame * 2;

    framesin = first_sample / samples_per_frame;

    /* 1 byte header: predictor = 1st, shift = 2nd */
    byte = (uint8_t)read_8bit(stream->offset+framesin*frame_size+0,stream->streamfile);
    predict_nr = byte >> 4;
    shift = byte & 0x0f;

    first_sample = first_sample % samples_per_frame;

	if (first_sample & 1) { /* if restarting on a high nibble, read byte first */
		byte = (uint8_t)read_8bit(stream->offset+(framesin*frame_size)+header_size+first_sample/2,stream->streamfile);
	}

    for (i = first_sample, sample_count = 0; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        sample = 0;

        if (predict_nr < 5) {
            if (!(i&1)) { /* low nibble first */
                byte = (uint8_t)read_8bit(stream->offset+(framesin*frame_size)+header_size+i/2,stream->streamfile);
                scale = (byte & 0x0f);
            } else { /* high nibble last */
                scale = byte >> 4;
            }
            scale = scale << 12; /* shift + sign extend (only if scale is int16_t) */
            /*if (scale > 7) {
                scale = scale - 16;
            }*/
#if VAG_USE_INTEGER_TABLE
            sample = (scale >> shift) +
                     (hist1 * VAG_coefs[predict_nr][0] +
                      hist2 * VAG_coefs[predict_nr][1] ) / 64;
#else
            sample = (int)( (scale >> shift) +
                     (hist1 * VAG_f[predict_nr][0] +
                      hist2 * VAG_f[predict_nr][1]) );
#endif
        }

        outbuf[sample_count] = clamp16(sample);
        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}


size_t ps_bytes_to_samples(size_t bytes, int channels) {
    return bytes / channels / 16 * 28;
}
