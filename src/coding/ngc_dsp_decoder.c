#include "coding.h"
#include "../util.h"

void decode_ngc_dsp(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i=first_sample;
    int32_t sample_count;

    int framesin = first_sample/14;

    int8_t header = read_8bit(framesin*8+stream->offset,stream->streamfile);
    int32_t scale = 1 << (header & 0xf);
    int coef_index = (header >> 4) & 0xf;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;
    int coef1 = stream->adpcm_coef[coef_index*2];
    int coef2 = stream->adpcm_coef[coef_index*2+1];

    first_sample = first_sample%14;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(framesin*8+stream->offset+1+i/2,stream->streamfile);

        outbuf[sample_count] = clamp16((
                 (((i&1?
                    get_low_nibble_signed(sample_byte):
                    get_high_nibble_signed(sample_byte)
                   ) * scale)<<11) + 1024 +
                 (coef1 * hist1 + coef2 * hist2))>>11
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}

/* read from memory rather than a file */
void decode_ngc_dsp_mem(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, uint8_t * mem) {
    int i=first_sample;
    int32_t sample_count;

    int framesin = first_sample/14;

    int8_t header = mem[framesin*8];
    int32_t scale = 1 << (header & 0xf);
    int coef_index = (header >> 4) & 0xf;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;
    int coef1 = stream->adpcm_coef[coef_index*2];
    int coef2 = stream->adpcm_coef[coef_index*2+1];

    first_sample = first_sample%14;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = mem[framesin*8+1+i/2];

        outbuf[sample_count] = clamp16((
                 (((i&1?
                    get_low_nibble_signed(sample_byte):
                    get_high_nibble_signed(sample_byte)
                   ) * scale)<<11) + 1024 +
                 (coef1 * hist1 + coef2 * hist2))>>11
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}

/*
 * The original DSP spec uses nibble counts for loop points, and some
 * variants don't have a proper sample count, so we (who are interested
 * in sample counts) need to do this conversion occasionally.
 */
int32_t dsp_nibbles_to_samples(int32_t nibbles) {
    int32_t whole_frames = nibbles/16;
    int32_t remainder = nibbles%16;

    /*
    fprintf(stderr,"%d (%#x) nibbles => %x bytes and %d samples\n",nibbles,nibbles,whole_frames*8,remainder);
    */

#if 0
    if (remainder > 0 && remainder < 14)
        return whole_frames*14 + remainder;
    else if (remainder >= 14)
        fprintf(stderr,"***** last frame %d leftover nibbles makes no sense\n",remainder);
#endif
    if (remainder>0) return whole_frames*14+remainder-2;
    else return whole_frames*14;
}

size_t dsp_bytes_to_samples(size_t bytes, int channels) {
    return bytes / channels / 8 * 14;
}


/**
 * reads DSP coefs built in the streamfile
 */
void dsp_read_coefs_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing) {
    dsp_read_coefs(vgmstream, streamFile, offset, spacing, 1);
}
void dsp_read_coefs_le(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing) {
    dsp_read_coefs(vgmstream, streamFile, offset, spacing, 0);
}
void dsp_read_coefs(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing, int be) {
    int ch, i;
    /* get ADPCM coefs */
    for (ch=0; ch < vgmstream->channels; ch++) {
        for (i=0; i < 16; i++) {
            vgmstream->ch[ch].adpcm_coef[i] = be ?
                    read_16bitBE(offset + ch*spacing + i*2, streamFile) :
                    read_16bitLE(offset + ch*spacing + i*2, streamFile);
        }
    }
}

