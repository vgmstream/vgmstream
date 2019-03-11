#include "coding.h"
#include "../util.h"

void decode_ngc_dsp(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
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
static void decode_ngc_dsp_subint_internal(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, uint8_t * mem) {
    int i=first_sample;
    int32_t sample_count;

    int8_t header = mem[0];
    int32_t scale = 1 << (header & 0xf);
    int coef_index = (header >> 4) & 0xf;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;
    int coef1 = stream->adpcm_coef[coef_index*2];
    int coef2 = stream->adpcm_coef[coef_index*2+1];

    first_sample = first_sample%14;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = mem[1 + i/2];

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

/* decode DSP with byte-interleaved frames (ex. 0x08: 1122112211221122) */
void decode_ngc_dsp_subint(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int interleave) {
    uint8_t sample_data[0x08];
    int i;

    int framesin = first_sample/14;

    for (i=0; i < 0x08; i++) {
        /* base + current frame + subint section + subint byte + channel adjust */
        sample_data[i] = read_8bit(
                stream->offset
                + framesin*(0x08*channelspacing)
                + i/interleave * interleave * channelspacing
                + i%interleave
                + interleave * channel, stream->streamfile);
    }

    decode_ngc_dsp_subint_internal(stream, outbuf, channelspacing, first_sample, samples_to_do, sample_data);
}


/*
 * The original DSP spec uses nibble counts for loop points, and some
 * variants don't have a proper sample count, so we (who are interested
 * in sample counts) need to do this conversion occasionally.
 */
int32_t dsp_nibbles_to_samples(int32_t nibbles) {
    int32_t whole_frames = nibbles/16;
    int32_t remainder = nibbles%16;

    if (remainder>0) return whole_frames*14+remainder-2;
    else return whole_frames*14;
}

size_t dsp_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 8 * 14;
}


/* reads DSP coefs built in the streamfile */
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

/* reads DSP initial hist built in the streamfile */
void dsp_read_hist_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing) {
    dsp_read_hist(vgmstream, streamFile, offset, spacing, 1);
}
void dsp_read_hist_le(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing) {
    dsp_read_hist(vgmstream, streamFile, offset, spacing, 0);
}
void dsp_read_hist(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing, int be) {
    int ch;
    /* get ADPCM hist */
    for (ch=0; ch < vgmstream->channels; ch++) {
        vgmstream->ch[ch].adpcm_history1_16 = be ?
                read_16bitBE(offset + ch*spacing + 0*2, streamFile) :
                read_16bitLE(offset + ch*spacing + 0*2, streamFile);;
        vgmstream->ch[ch].adpcm_history2_16 = be ?
                read_16bitBE(offset + ch*spacing + 1*2, streamFile) :
                read_16bitLE(offset + ch*spacing + 1*2, streamFile);;
    }
}

