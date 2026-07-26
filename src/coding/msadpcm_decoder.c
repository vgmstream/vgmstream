#include "../util.h"
#include "coding.h"

/* table values are interpreted as fixed point 8.8 signed values */ 

/* AdaptionTable */
static const int16_t msadpcm_steps[16] = {
    230, 230, 230, 230,
    307, 409, 512, 614,
    768, 614, 512, 409,
    307, 230, 230, 230
};

#define MSADPCM_MAX_COEFFICIENTS 7

/* aCoeff table, normally included with container but in practice always fixed to these.
 * In theory the encoder may add extra coefs too (nNumCoef). */
static const int16_t msadpcm_coefs[7][2] = {
    { 256,    0 },
    { 512, -256 },
    {   0,    0 },
    { 192,   64 },
    { 240,    0 },
    { 460, -208 },
    { 392, -232 }
};

/* Decodes MSADPCM as explained in the spec (RIFFNEW doc + msadpcm.c).
 * Though RIFFNEW writes "predictor / 256" (DIV), msadpcm.c uses "predictor >> 8" (SHR). They may seem the
 * same but on negative values SHR gets different results (-128 / 256 = 0; -128 >> 8 = -1) = some output diffs.
 * SHR is true in Windows msadp32.acm decoders (up to Win10), while some non-Windows implementations or
 * engines (like UE4) may use DIV.
 *
 * On invalid coef index, msadpcm.c returns 0 decoded samples but here we clamp and keep on trucking.
 * In theory blocks may be 0-padded and should use samples_per_frame from header, in practice seems to
 * decode up to block length or available data. */

static int16_t msadpcm_adpcm_expand_nibble_shr(VGMSTREAMCHANNEL* stream, uint8_t byte, int shift) {
    int32_t hist1, hist2, predicted;
    int code = (shift) ?
         get_high_nibble_signed(byte) :
         get_low_nibble_signed (byte);

    hist1 = stream->adpcm_history1_16;
    hist2 = stream->adpcm_history2_16;
    predicted = hist1 * stream->adpcm_coef[0] + hist2 * stream->adpcm_coef[1];
    predicted = predicted >> 8; /* 256 = FIXED_POINT_COEF_BASE (uses SHR instead) */
    predicted = predicted + (code * stream->adpcm_scale);
    predicted = clamp16(predicted); /* lNewSample */

    stream->adpcm_history2_16 = stream->adpcm_history1_16;
    stream->adpcm_history1_16 = predicted;

    stream->adpcm_scale = (msadpcm_steps[code & 0xf] * stream->adpcm_scale) >> 8; /* not diffs vs DIV here (always >=0) */
    if (stream->adpcm_scale < 16) /* min delta */
        stream->adpcm_scale = 16;

    return predicted;
}

static int16_t msadpcm_adpcm_expand_nibble_div(VGMSTREAMCHANNEL* stream, uint8_t byte, int shift) {
    int32_t hist1, hist2, predicted;

    int code = (shift) ?
         get_high_nibble_signed(byte) :
         get_low_nibble_signed (byte);

    hist1 = stream->adpcm_history1_16;
    hist2 = stream->adpcm_history2_16;
    predicted = hist1 * stream->adpcm_coef[0] + hist2 * stream->adpcm_coef[1];
    predicted = predicted / 256; /* 256 = FIXED_POINT_COEF_BASE */
    predicted = predicted + (code * stream->adpcm_scale);
    predicted = clamp16(predicted); /* lNewSample */

    stream->adpcm_history2_16 = stream->adpcm_history1_16;
    stream->adpcm_history1_16 = predicted;

    stream->adpcm_scale = (msadpcm_steps[code & 0xf] * stream->adpcm_scale) / 256; /* 256 = FIXED_POINT_ADAPTION_BASE */
    if (stream->adpcm_scale < 16) /* min delta */
        stream->adpcm_scale = 16;

    return predicted;
}

static int get_index(uint8_t* frame, int num_coefs) {
    int index = get_u8(frame);

    if (index >= num_coefs) {
        VGM_LOG("MSADPCM: invalid coefs index=%i\n", index);
        index = 0; // OG code stops decoding instead
    }

    return index;
}

void decode_msadpcm_stereo(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t first_sample, int32_t samples_to_do) {
    VGMSTREAMCHANNEL* stream1 = &vgmstream->ch[0];
    VGMSTREAMCHANNEL* stream2 = &vgmstream->ch[1];
    uint8_t frame[MSADPCM_MAX_BLOCK_SIZE] = {0};
    const int num_coefs = MSADPCM_MAX_COEFFICIENTS;

    /* external interleave (variable size), stereo */
    size_t bytes_per_frame = vgmstream->frame_size;
    size_t samples_per_frame = (vgmstream->frame_size - 0x07 * vgmstream->channels) * 2 / vgmstream->channels + 2;
    int frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    off_t frame_offset = stream1->offset + frames_in * bytes_per_frame;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream1->streamfile); /* ignore EOF errors */

    // predictor >= nNumCoef > return 0

    /* parse frame header (ADPCMBLOCKHEADER) */
    if (first_sample == 0) {
        int index_l = get_index(frame+0x00, num_coefs);
        int index_r = get_index(frame+0x01, num_coefs);
        stream1->adpcm_coef[0] = msadpcm_coefs[index_l][0]; /* bPredictor[0] index > iCoef1 */
        stream1->adpcm_coef[1] = msadpcm_coefs[index_l][1]; /* bPredictor[0] index > iCoef2 */
        stream2->adpcm_coef[0] = msadpcm_coefs[index_r][0]; /* bPredictor[1] index > iCoef1 */
        stream2->adpcm_coef[1] = msadpcm_coefs[index_r][1]; /* bPredictor[1] index > iCoef2 */
        stream1->adpcm_scale = get_s16le(frame+0x02); /* iDelta[0] */
        stream2->adpcm_scale = get_s16le(frame+0x04); /* iDelta[0] */
        stream1->adpcm_history1_16 = get_s16le(frame+0x06); /* iSamp1[0] */
        stream2->adpcm_history1_16 = get_s16le(frame+0x08); /* iSamp1[0] */
        stream1->adpcm_history2_16 = get_s16le(frame+0x0a); /* iSamp2[0] */
        stream2->adpcm_history2_16 = get_s16le(frame+0x0c); /* iSamp2[1] */
    }

    /* write header samples (needed) */
    if (first_sample == 0) {
        outbuf[0] = stream1->adpcm_history2_16;
        outbuf[1] = stream2->adpcm_history2_16;
        outbuf += 2;
        first_sample++;
        samples_to_do--;
    }
    if (first_sample == 1 && samples_to_do > 0) {
        outbuf[0] = stream1->adpcm_history1_16;
        outbuf[1] = stream2->adpcm_history1_16;
        outbuf += 2;
        first_sample++;
        samples_to_do--;
    }

    /* decode nibbles */
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t byte = get_u8(frame+0x07*2+(i-2));

        *outbuf++ = msadpcm_adpcm_expand_nibble_shr(&vgmstream->ch[0], byte, 1); /* L */
        *outbuf++ = msadpcm_adpcm_expand_nibble_shr(&vgmstream->ch[1], byte, 0); /* R */
    }
}

void decode_msadpcm_mono(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int config) {
    VGMSTREAMCHANNEL* stream = &vgmstream->ch[channel];
    uint8_t frame[MSADPCM_MAX_BLOCK_SIZE] = {0};
    const int num_coefs = MSADPCM_MAX_COEFFICIENTS;
    bool is_shr = (config == 0);

    /* external interleave (variable size), mono */
    size_t bytes_per_frame = vgmstream->frame_size;
    size_t samples_per_frame = (vgmstream->frame_size - 0x07) * 2 + 2;
    int frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    off_t frame_offset = stream->offset + frames_in * bytes_per_frame;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    /* parse frame header */
    if (first_sample == 0) {
        int index = get_index(frame+0x00, num_coefs);
        stream->adpcm_coef[0] = msadpcm_coefs[index][0];
        stream->adpcm_coef[1] = msadpcm_coefs[index][1];
        stream->adpcm_scale = get_s16le(frame+0x01);
        stream->adpcm_history1_16 = get_s16le(frame+0x03);
        stream->adpcm_history2_16 = get_s16le(frame+0x05);
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
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t byte = get_u8(frame+0x07+(i-2)/2);
        int shift = !(i & 1); /* high nibble first */

        outbuf[0] = is_shr ?
                msadpcm_adpcm_expand_nibble_shr(stream, byte, shift) :
                msadpcm_adpcm_expand_nibble_div(stream, byte, shift);
        outbuf += channelspacing;
    }
}

/* Cricket Audio's MSADPCM, same thing with reversed hist and nibble order, reverse engineered from the exe.
 * (their tools may convert to float/others but internally it's all PCM16). */
void decode_msadpcm_ck(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL* stream = &vgmstream->ch[channel];
    uint8_t frame[MSADPCM_MAX_BLOCK_SIZE] = {0};
    const int num_coefs = MSADPCM_MAX_COEFFICIENTS;

    /* external interleave (variable size), mono */
    size_t bytes_per_frame = vgmstream->frame_size;
    size_t samples_per_frame = (vgmstream->frame_size - 0x07) * 2 + 2;
    int frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    off_t frame_offset = stream->offset + frames_in * bytes_per_frame;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    /* parse frame header */
    if (first_sample == 0) {
        int index = get_index(frame+0x00, num_coefs);
        stream->adpcm_coef[0] = msadpcm_coefs[index][0];
        stream->adpcm_coef[1] = msadpcm_coefs[index][1];
        stream->adpcm_scale = get_s16le(frame+0x01);
        stream->adpcm_history2_16 = get_s16le(frame+0x03); /* hist2 first, unlike normal MSADPCM */
        stream->adpcm_history1_16 = get_s16le(frame+0x05);
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
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t byte = get_u8(frame+0x07+(i-2)/2);
        int shift = (i & 1); /* low nibble first, unlike normal MSADPCM */

        outbuf[0] = msadpcm_adpcm_expand_nibble_shr(stream, byte, shift);
        outbuf += channelspacing;
    }
}

long msadpcm_bytes_to_samples(long bytes, int block_size, int channels) {
    if (block_size <= 0 || channels <= 0)
        return 0;
    return (bytes / block_size) * (block_size - (7-1)*channels) * 2 / channels
            + ((bytes % block_size) ? ((bytes % block_size) - (7-1)*channels) * 2 / channels : 0);
}

/* test if MSADPCM coefs were re-defined (possible in theory but not used in practice) */
bool msadpcm_check_coefs(STREAMFILE* sf, uint32_t offset) {
    int num_coefs = read_u16le(offset, sf); // nNumCoef
    if (num_coefs != MSADPCM_MAX_COEFFICIENTS) {
        vgm_logi("MSADPCM: bad count %i at %x (report)\n", num_coefs, offset);
        return false;
    }

    offset += 0x02;
    for (int i = 0; i < MSADPCM_MAX_COEFFICIENTS; i++) {
        int16_t coef1 = read_s16le(offset + 0x00, sf);
        int16_t coef2 = read_s16le(offset + 0x02, sf);

        if (coef1 != msadpcm_coefs[i][0] || coef2 != msadpcm_coefs[i][1]) {
            vgm_logi("MSADPCM: bad coef %i/%i vs %i/%i (report)\n", coef1, coef2, msadpcm_coefs[i][0], msadpcm_coefs[i][1]);
            return false;
        }
        offset += 0x02 + 0x02;
    }

    return true;
}
