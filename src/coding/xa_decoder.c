#include "coding.h"
#include "../util.h"

// todo this is based on Kazzuya's old code; different emus (PCSX, Mame, Mednafen, etc) do
//  XA coefs int math in different ways (see comments below), not 100% accurate.
// May be implemented like the SNES/SPC700 BRR.

/* XA ADPCM gain values */
static const double K0[4] = { 0.0, 0.9375, 1.796875,  1.53125 };
static const double K1[4] = { 0.0,    0.0,  -0.8125,-0.859375};
/* K0/1 floats to int, K*2^10 = K*(1<<10) = K*1024 */
static int get_IK0(int fid) { return ((int)((-K0[fid]) * (1 << 10))); }
static int get_IK1(int fid) { return ((int)((-K1[fid]) * (1 << 10))); }

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
 * differently, maybe using one of the above methods in software/CPU, but in XA's case may be done
 * like the SNES/SPC700 BRR, with specific per-filter ops.
 * int coef tables commonly use N = 6 or 8, so K0 0.9375*64 = 60 or 0.9375*256 = 240
 * PS1 XA is apparently upsampled and interpolated to 44100, vgmstream doesn't simulate this.
 *
 * Info (Green Book): https://www.lscdweb.com/data/downloadables/2/8/cdi_may94_r2.pdf
 * BRR info (no$sns): http://problemkaputt.de/fullsnes.htm#snesapudspbrrsamples
 *           (bsnes): https://gitlab.com/higan/higan/blob/master/higan/sfc/dsp/brr.cpp
 */

void decode_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    off_t frame_offset, sp_offset;
    int i,j, frames_in, samples_done = 0, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (fixed size), mono/stereo */
    bytes_per_frame = 0x80;
    samples_per_frame = 28*8 / channelspacing;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* data layout (mono):
     * - CD-XA audio is divided into sectors ("audio blocks"), each with 18 size 0x80 frames
     *   (handled externally, this decoder only gets frames)
     * - a frame ("sound group") is divided into 8 subframes ("sound unit"), with
     *   subframe headers ("sound parameters") first then subframe nibbles ("sound data")
     * - headers: 0..3 + repeat 0..3 + 4..7 + repeat 4..7 (where N = subframe N header)
     *   (repeats may be for error correction, though probably unused)
     * - nibbles: 32b with nibble0 for subframes 0..8, 32b with nibble1 for subframes 0..8, etc
     *   (low first: 32b = sf1-n0 sf0-n0  sf3-n0 sf2-n0  sf5-n0 sf4-n0  sf7-n0 sf6-n0, etc)
     *
     * stereo layout is the same but alternates channels: subframe 0/2/4/6=L, subframe 1/3/5/7=R
     *
     * example:
     *   subframe 0: header @ 0x00 or 0x04, 28 nibbles (low)  @ 0x10,14,18,1c,20 ... 7c
     *   subframe 1: header @ 0x01 or 0x05, 28 nibbles (high) @ 0x10,14,18,1c,20 ... 7c
     *   subframe 2: header @ 0x02 or 0x06, 28 nibbles (low)  @ 0x11,15,19,1d,21 ... 7d
     *   ...
     *   subframe 7: header @ 0x0b or 0x0f, 28 nibbles (high) @ 0x13,17,1b,1f,23 ... 7f
     */
    frame_offset = stream->offset + bytes_per_frame*frames_in;

    if (read_32bitBE(frame_offset+0x00,stream->streamfile) != read_32bitBE(frame_offset+0x04,stream->streamfile) ||
        read_32bitBE(frame_offset+0x08,stream->streamfile) != read_32bitBE(frame_offset+0x0c,stream->streamfile)) {
        VGM_LOG("bad frames at %x\n", (uint32_t)frame_offset);
    }


    /* decode subframes */
    for (i = 0; i < 8 / channelspacing; i++) {
        int32_t coef1, coef2;
        uint8_t coef_index, shift_factor;

        /* parse current subframe (sound unit)'s header (sound parameters) */
        sp_offset = frame_offset + 0x04 + i*channelspacing + channel;
        coef_index   = ((uint8_t)read_8bit(sp_offset,stream->streamfile) >> 4) & 0xf;
        shift_factor = ((uint8_t)read_8bit(sp_offset,stream->streamfile) >> 0) & 0xf;

        VGM_ASSERT(coef_index > 4 || shift_factor > 12, "XA: incorrect coefs/shift at %x\n", (uint32_t)sp_offset);
        if (coef_index > 4)
            coef_index = 0; /* only 4 filters are used, rest is apparently 0 */
        if (shift_factor > 12)
            shift_factor = 9; /* supposedly, from Nocash PSX docs */

        coef1 = get_IK0(coef_index);
        coef2 = get_IK1(coef_index);


        /* decode subframe nibbles */
        for(j = 0; j < 28; j++) {
            uint8_t nibbles;
            int32_t new_sample;

            off_t su_offset = (channelspacing==1) ?
                    frame_offset + 0x10 + j*0x04 + (i/2) : /* mono */
                    frame_offset + 0x10 + j*0x04 + i;      /* stereo */
            int get_high_nibble = (channelspacing==1) ?
                    (i&1) :         /* mono (even subframes = low, off subframes = high) */
                    (channel == 1); /* stereo (L channel / even subframes = low, R channel / odd subframes = high) */

            /* skip half decodes to make sure hist isn't touched (kinda hack-ish) */
            if (!(sample_count >= first_sample && samples_done < samples_to_do)) {
                sample_count++;
                continue;
            }

            nibbles = (uint8_t)read_8bit(su_offset,stream->streamfile);

            new_sample = get_high_nibble ?
                    (nibbles >> 4) & 0x0f :
                    (nibbles     ) & 0x0f;

            new_sample = (int16_t)((new_sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
            new_sample = new_sample << 4;
            new_sample = new_sample - ((coef1*hist1 + coef2*hist2) >> 10);

            hist2 = hist1;
            hist1 = new_sample; /* must go before clamp, somehow */
            new_sample = new_sample >> 4;
            new_sample = clamp16(new_sample);

            outbuf[samples_done * channelspacing] = new_sample;
            samples_done++;

            sample_count++;
        }
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

size_t xa_bytes_to_samples(size_t bytes, int channels, int is_blocked) {
    if (is_blocked) {
        return (bytes / 0x930) * (28*8/ channels) * 18;
    }
    else {
        return (bytes / 0x80) * (28*8 / channels);
    }
}
