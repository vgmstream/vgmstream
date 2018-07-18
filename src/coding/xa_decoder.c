#include "coding.h"
#include "../util.h"

// todo this is based on Kazzuya's old code; different emus (PCSX, Mame, Mednafen, etc) do
//  XA coefs int math in different ways (see comments below), not be 100% accurate.
// May be implemented like the SNES/SPC700 BRR (see BSNES' brr.cpp, hardware-tested).

/* XA ADPCM gain values */
static const double K0[4] = { 0.0, 0.9375, 1.796875,  1.53125 };
static const double K1[4] = { 0.0,    0.0,  -0.8125,-0.859375};
static int IK0(int fid) { return ((int)((-K0[fid]) * (1 << 10))); } /* K0/1 floats to int, K*2^10 = K*(1<<10) = K*1024 */
static int IK1(int fid) { return ((int)((-K1[fid]) * (1 << 10))); }

/* Sony XA ADPCM, defined for CD-DA/CD-i in the "Red Book" (private) or "Green Book" (public) specs.
 * The algorithm basically is BRR (Bit Rate Reduction) from the SNES SPC700, while the data layout is new.
 *
 * Decoding is defined in diagrams, roughly as:
 *     pcm = clamp( signed_nibble * 2^(12-range) + K0[index]*hist1 + K1[index]*hist2 )
 * - Range (12-range=shift) and filter index are renewed every ~28 samples.
 * - nibble is expanded to a signed 16b sample, reimplemented as:
 *     short sample = ((nibble << 12) & 0xf000) >> shift
 *     or: int sample = ((nibble << 28) & 0xf0000000) >> (shift + N)
 * - K0/K1 are float coefs are typically redefined with int math in various ways, with non-equivalent rounding:
 *     (sample + K0*2^N*hist1 + K1*2^N*hist2 + [(2^N)/2]) / 2^N
 *     (sample + K0*2^N*hist1 + K1*2^N*hist2 + [(2^N)/2]) >> N
 *     sample + (K0*2^N*hist1 + K1*2^N*hist2)>>N
 *     sample + (K0*2^N*hist1)>>N + (K1*2^N*hist2)>>N
 *     etc
 *   (rounding differences should be inaudible, so public implementations may be approximations)
 *
 * Various XA descendants (PS-ADPCM, EA-XA, NGC DTK, FADPCM, etc) do filters/rounding slightly
 * differently, using one of the above methods in software/CPU, but in XA's case may be done like
 * the SNES/SPC700 BRR, with specific per-filter ops.
 * int coef tables commonly use N = 6 or 8, so K0 0.9375*64 = 60 or 0.9375*256 = 240
 * PS1 XA is apparently upsampled and interpolated to 44100, vgmstream doesn't simulate this.
 *
 * Info (Green Book): https://www.lscdweb.com/data/downloadables/2/8/cdi_may94_r2.pdf
 */
void decode_xa(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    static int head_table[8] = {0,2,8,10};
    VGMSTREAMCHANNEL * stream = &(vgmstream->ch[channel]);
    off_t sp_offset;
    int i;
    int frames_in, sample_count = 0;
    int32_t coef1, coef2, coef_index, shift_factor;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (fixed size), mono/stereo */
    frames_in = first_sample / (28*2 / channelspacing);
    first_sample = first_sample % 28;

    /* hack for mono/stereo handling */
    vgmstream->xa_get_high_nibble = !vgmstream->xa_get_high_nibble;
    if (first_sample && channelspacing==1)
        vgmstream->xa_get_high_nibble = !vgmstream->xa_get_high_nibble;

    /* parse current sound unit (subframe) sound parameters */
    sp_offset = stream->offset+head_table[frames_in]+vgmstream->xa_get_high_nibble;
    coef_index   = (read_8bit(sp_offset,stream->streamfile) >> 4) & 0xf;
    shift_factor = (read_8bit(sp_offset,stream->streamfile)     ) & 0xf;

    VGM_ASSERT(coef_index > 4 || shift_factor > 12, "XA: incorrect coefs/shift at %lx\n", sp_offset);
    if (coef_index > 4)
        coef_index = 0; /* only 4 filters are used, rest is apparently 0 */
    if (shift_factor > 12)
        shift_factor = 9; /* supposedly, from Nocash PSX docs */

    coef1 = IK0(coef_index);
    coef2 = IK1(coef_index);


    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample;
        uint8_t nibbles = (uint8_t)read_8bit(stream->offset+0x10+frames_in+(i*0x04),stream->streamfile);

        new_sample = vgmstream->xa_get_high_nibble ?
                (nibbles >> 4) & 0x0f :
                (nibbles     ) & 0x0f;
        new_sample = (int16_t)((new_sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
        new_sample = new_sample << 4;
        new_sample = new_sample - ((coef1*hist1 + coef2*hist2) >> 10);

        hist2 = hist1;
        hist1 = new_sample; /* must go before clamp, somehow */
        new_sample = new_sample >> 4;
        new_sample = clamp16(new_sample);

        outbuf[sample_count] = new_sample;
        sample_count += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

size_t xa_bytes_to_samples(size_t bytes, int channels, int is_blocked) {
    if (is_blocked) {
        //todo with -0x10 misses the last sector, not sure if bug or feature
        return ((bytes - 0x10) / 0x930) * (0x900 - 18*0x10) * 2 / channels;
    }
    else {
        return ((bytes / 0x80)*0xE0) / 2;
    }
}
