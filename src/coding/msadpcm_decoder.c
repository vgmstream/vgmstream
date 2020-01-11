#include "../util.h"
#include "coding.h"


static const int16_t msadpcm_steps[16] = {
    230, 230, 230, 230,
    307, 409, 512, 614,
    768, 614, 512, 409,
    307, 230, 230, 230
};

static const int16_t msadpcm_coefs[7][2] = {
    { 256,    0 },
    { 512, -256 },
    {   0,    0 },
    { 192,   64 },
    { 240,    0 },
    { 460, -208 },
    { 392, -232 }
};

void decode_msadpcm_stereo(VGMSTREAM * vgmstream, sample_t * outbuf, int32_t first_sample, int32_t samples_to_do) {
    VGMSTREAMCHANNEL *ch1,*ch2;
    STREAMFILE *streamfile;
    int i, frames_in;
    size_t bytes_per_frame, samples_per_frame;
    off_t frame_offset;

    ch1 = &vgmstream->ch[0];
    ch2 = &vgmstream->ch[1];
    streamfile = ch1->streamfile;

    /* external interleave (variable size), stereo */
    bytes_per_frame = get_vgmstream_frame_size(vgmstream);
    samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    frame_offset = ch1->offset + frames_in*bytes_per_frame;

    /* parse frame header */
    if (first_sample == 0) {
        ch1->adpcm_coef[0] = msadpcm_coefs[read_8bit(frame_offset+0x00,streamfile) & 0x07][0];
        ch1->adpcm_coef[1] = msadpcm_coefs[read_8bit(frame_offset+0x00,streamfile) & 0x07][1];
        ch2->adpcm_coef[0] = msadpcm_coefs[read_8bit(frame_offset+0x01,streamfile)][0];
        ch2->adpcm_coef[1] = msadpcm_coefs[read_8bit(frame_offset+0x01,streamfile)][1];
        ch1->adpcm_scale = read_16bitLE(frame_offset+0x02,streamfile);
        ch2->adpcm_scale = read_16bitLE(frame_offset+0x04,streamfile);
        ch1->adpcm_history1_16 = read_16bitLE(frame_offset+0x06,streamfile);
        ch2->adpcm_history1_16 = read_16bitLE(frame_offset+0x08,streamfile);
        ch1->adpcm_history2_16 = read_16bitLE(frame_offset+0x0a,streamfile);
        ch2->adpcm_history2_16 = read_16bitLE(frame_offset+0x0c,streamfile);
    }

    /* write header samples (needed) */
    if (first_sample==0) {
        outbuf[0] = ch1->adpcm_history2_16;
        outbuf[1] = ch2->adpcm_history2_16;
        outbuf += 2;
        first_sample++;
        samples_to_do--;
    }
    if (first_sample == 1 && samples_to_do > 0) {
        outbuf[0] = ch1->adpcm_history1_16;
        outbuf[1] = ch2->adpcm_history1_16;
        outbuf += 2;
        first_sample++;
        samples_to_do--;
    }

    /* decode nibbles */
    for (i = first_sample; i < first_sample+samples_to_do; i++) {
        int ch;

        for (ch = 0; ch < 2; ch++) {
            VGMSTREAMCHANNEL *stream = &vgmstream->ch[ch];
            int32_t hist1,hist2, predicted;
            int sample_nibble = (ch == 0) ? /* L = high nibble first */
                 get_high_nibble_signed(read_8bit(frame_offset+0x07*2+(i-2),streamfile)) :
                 get_low_nibble_signed (read_8bit(frame_offset+0x07*2+(i-2),streamfile));

            hist1 = stream->adpcm_history1_16;
            hist2 = stream->adpcm_history2_16;
            predicted = hist1*stream->adpcm_coef[0] + hist2*stream->adpcm_coef[1];
            predicted = predicted / 256;
            predicted = predicted + sample_nibble*stream->adpcm_scale;
            outbuf[0] = clamp16(predicted);

            stream->adpcm_history2_16 = stream->adpcm_history1_16;
            stream->adpcm_history1_16 = outbuf[0];
            stream->adpcm_scale = (msadpcm_steps[sample_nibble & 0xf] * stream->adpcm_scale) / 256;
            if (stream->adpcm_scale < 0x10)
                stream->adpcm_scale = 0x10;

            outbuf++;
        }
    }
}

void decode_msadpcm_mono(VGMSTREAM * vgmstream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL *stream = &vgmstream->ch[channel];
    int i, frames_in;
    size_t bytes_per_frame, samples_per_frame;
    off_t frame_offset;

    /* external interleave (variable size), mono */
    bytes_per_frame = get_vgmstream_frame_size(vgmstream);
    samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    frame_offset = stream->offset + frames_in*bytes_per_frame;

    /* parse frame header */
    if (first_sample == 0) {
        stream->adpcm_coef[0] = msadpcm_coefs[read_8bit(frame_offset+0x00,stream->streamfile) & 0x07][0];
        stream->adpcm_coef[1] = msadpcm_coefs[read_8bit(frame_offset+0x00,stream->streamfile) & 0x07][1];
        stream->adpcm_scale = read_16bitLE(frame_offset+0x01,stream->streamfile);
        stream->adpcm_history1_16 = read_16bitLE(frame_offset+0x03,stream->streamfile);
        stream->adpcm_history2_16 = read_16bitLE(frame_offset+0x05,stream->streamfile);
    }

    /* write header samples (needed) */
    if (first_sample == 0) {
        outbuf[0] = stream->adpcm_history2_16;
        outbuf += channelspacing;
        first_sample++;
        samples_to_do--;
    }
    if (first_sample == 1 && samples_to_do > 0) {
        outbuf[0] = stream->adpcm_history1_16;
        outbuf += channelspacing;
        first_sample++;
        samples_to_do--;
    }

    /* decode nibbles */
    for (i = first_sample; i < first_sample+samples_to_do; i++) {
        int32_t hist1,hist2, predicted;
        int sample_nibble = (i & 1) ? /* high nibble first */
             get_low_nibble_signed (read_8bit(frame_offset+0x07+(i-2)/2,stream->streamfile)) :
             get_high_nibble_signed(read_8bit(frame_offset+0x07+(i-2)/2,stream->streamfile));

        hist1 = stream->adpcm_history1_16;
        hist2 = stream->adpcm_history2_16;
        predicted = hist1*stream->adpcm_coef[0] + hist2*stream->adpcm_coef[1];
        predicted = predicted / 256;
        predicted = predicted + sample_nibble*stream->adpcm_scale;
        outbuf[0] = clamp16(predicted);

        stream->adpcm_history2_16 = stream->adpcm_history1_16;
        stream->adpcm_history1_16 = outbuf[0];
        stream->adpcm_scale = (msadpcm_steps[sample_nibble & 0xf] * stream->adpcm_scale) / 256;
        if (stream->adpcm_scale < 0x10)
            stream->adpcm_scale = 0x10;

        outbuf += channelspacing;
    }
}

/* Cricket Audio's MSADPCM, same thing with reversed hist and nibble order
 * (their tools may convert to float/others but internally it's all PCM16, from debugging). */
void decode_msadpcm_ck(VGMSTREAM * vgmstream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL *stream = &vgmstream->ch[channel];
    int i, frames_in;
    size_t bytes_per_frame, samples_per_frame;
    off_t frame_offset;

    /* external interleave (variable size), mono */
    bytes_per_frame = get_vgmstream_frame_size(vgmstream);
    samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    frame_offset = stream->offset + frames_in*bytes_per_frame;

    /* parse frame header */
    if (first_sample == 0) {
        stream->adpcm_coef[0] = msadpcm_coefs[read_8bit(frame_offset+0x00,stream->streamfile) & 0x07][0];
        stream->adpcm_coef[1] = msadpcm_coefs[read_8bit(frame_offset+0x00,stream->streamfile) & 0x07][1];
        stream->adpcm_scale = read_16bitLE(frame_offset+0x01,stream->streamfile);
        stream->adpcm_history2_16 = read_16bitLE(frame_offset+0x03,stream->streamfile); /* hist2 first, unlike normal MSADPCM */
        stream->adpcm_history1_16 = read_16bitLE(frame_offset+0x05,stream->streamfile);
    }

    /* write header samples (needed) */
    if (first_sample == 0) {
        outbuf[0] = stream->adpcm_history2_16;
        outbuf += channelspacing;
        first_sample++;
        samples_to_do--;
    }
    if (first_sample == 1 && samples_to_do > 0) {
        outbuf[0] = stream->adpcm_history1_16;
        outbuf += channelspacing;
        first_sample++;
        samples_to_do--;
    }

    /* decode nibbles */
    for (i = first_sample; i < first_sample+samples_to_do; i++) {
        int32_t hist1,hist2, predicted;
        int sample_nibble = (i & 1) ? /* low nibble first, unlike normal MSADPCM */
             get_high_nibble_signed (read_8bit(frame_offset+0x07+(i-2)/2,stream->streamfile)) :
             get_low_nibble_signed(read_8bit(frame_offset+0x07+(i-2)/2,stream->streamfile));

        hist1 = stream->adpcm_history1_16;
        hist2 = stream->adpcm_history2_16;
        predicted = hist1*stream->adpcm_coef[0] + hist2*stream->adpcm_coef[1];
        predicted = predicted >> 8; /* probably no difference vs MSADPCM */
        predicted = predicted + sample_nibble*stream->adpcm_scale;
        outbuf[0] = clamp16(predicted);

        stream->adpcm_history2_16 = stream->adpcm_history1_16;
        stream->adpcm_history1_16 = outbuf[0];
        stream->adpcm_scale = (msadpcm_steps[sample_nibble & 0xf] * stream->adpcm_scale) >> 8;
        if (stream->adpcm_scale < 0x10)
            stream->adpcm_scale = 0x10;

        outbuf += channelspacing;
    }
}

long msadpcm_bytes_to_samples(long bytes, int block_size, int channels) {
    if (block_size <= 0 || channels <= 0) return 0;
    return (bytes / block_size) * (block_size - (7-1)*channels) * 2 / channels
            + ((bytes % block_size) ? ((bytes % block_size) - (7-1)*channels) * 2 / channels : 0);
}

/* test if MSADPCM coefs were re-defined (possible in theory but not used in practice) */
int msadpcm_check_coefs(STREAMFILE *sf, off_t offset) {
    int i;
    int count = read_16bitLE(offset, sf);
    if (count != 7) {
        VGM_LOG("MSADPCM: bad count %i at %lx\n", count, offset);
        goto fail;
    }

    offset += 0x02;
    for (i = 0; i < 7; i++) {
        int16_t coef1 = read_16bitLE(offset + 0x00, sf);
        int16_t coef2 = read_16bitLE(offset + 0x02, sf);

        if (coef1 != msadpcm_coefs[i][0] || coef2 != msadpcm_coefs[i][1]) {
            VGM_LOG("MSADPCM: bad coef %i/%i vs %i/%i\n", coef1, coef2, msadpcm_coefs[i][0], msadpcm_coefs[i][1]);
            goto fail;
        }
        offset += 0x02 + 0x02;
    }

    return 1;
fail:
    return 0;
}
