#include "coding.h"
#include "../util.h"


void decode_ngc_dsp(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x08] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int coef_index, scale, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x08;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* always 14 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    scale = 1 << ((frame[0] >> 0) & 0xf);
    coef_index  = (frame[0] >> 4) & 0xf;

    VGM_ASSERT_ONCE(coef_index > 8, "DSP: incorrect coefs at %x\n", (uint32_t)frame_offset);
    //if (coef_index > 8) //todo not correctly clamped in original decoder?
    //    coef_index = 8;

    coef1 = stream->adpcm_coef[coef_index*2 + 0];
    coef2 = stream->adpcm_coef[coef_index*2 + 1];


    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample = 0;
        uint8_t nibbles = frame[0x01 + i/2];

        sample = i&1 ? /* high nibble first */
                get_low_nibble_signed(nibbles) :
                get_high_nibble_signed(nibbles);
        sample = ((sample * scale) << 11);
        sample = (sample + 1024 + coef1*hist1 + coef2*hist2) >> 11;
        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}


/* read from memory rather than a file */
static void decode_ngc_dsp_subint_internal(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, uint8_t * frame) {
    int i, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int coef_index, scale, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x08;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* always 14 */
    first_sample = first_sample % samples_per_frame;
    VGM_ASSERT_ONCE(samples_to_do > samples_per_frame, "DSP: layout error, too many samples\n");

    /* parse frame header */
    scale = 1 << ((frame[0] >> 0) & 0xf);
    coef_index  = (frame[0] >> 4) & 0xf;

    VGM_ASSERT_ONCE(coef_index > 8, "DSP: incorrect coefs\n");
    //if (coef_index > 8) //todo not correctly clamped in original decoder?
    //    coef_index = 8;

    coef1 = stream->adpcm_coef[coef_index*2 + 0];
    coef2 = stream->adpcm_coef[coef_index*2 + 1];

    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample = 0;
        uint8_t nibbles = frame[0x01 + i/2];

        sample = i&1 ?
                get_low_nibble_signed(nibbles) :
                get_high_nibble_signed(nibbles);
        sample = ((sample * scale) << 11);
        sample = (sample + 1024 + coef1*hist1 + coef2*hist2) >> 11;
        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}

/* decode DSP with byte-interleaved frames (ex. 0x08: 1122112211221122) */
void decode_ngc_dsp_subint(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int interleave) {
    uint8_t frame[0x08];
    int i;
    int frames_in = first_sample / 14;

    for (i = 0; i < 0x08; i++) {
        /* base + current frame + subint section + subint byte + channel adjust */
        frame[i] = read_8bit(
                stream->offset
                + frames_in*(0x08*channelspacing)
                + i/interleave * interleave * channelspacing
                + i%interleave
                + interleave * channel, stream->streamfile);
    }

    decode_ngc_dsp_subint_internal(stream, outbuf, channelspacing, first_sample, samples_to_do, frame);
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

