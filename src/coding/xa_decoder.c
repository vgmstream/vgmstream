#include "coding.h"
#include "../util.h"

/* XA ADPCM gain values */
//#define XA_FLOAT 1
#if XA_FLOAT
/* floats as defined by the spec, but PS1's SPU would use int math */
static const float K0[4+12] = { 0.0, 0.9375, 1.796875, 1.53125 };
static const float K1[4+12] = { 0.0,    0.0,  -0.8125, -0.859375 };
#else
/* K0/1 floats to int with N=6: K*2^6 = K*(1<<6) = K*64 (upper ranges are supposedly 0)*/
static const int K0[4+12] = {  0,   60,  115,  98 };
static const int K1[4+12] = {  0,    0,  -52, -55 };
#endif

/* EA's extended XA (N=8), reverse engineered from SAT exes. Basically the same with minor
 * diffs and extra steps probably for the SH2 CPU (only does 1/2/8 shifts) */
static const int16_t EA_TABLE[16][2] = {
    {   0,    0 },
    { 240,    0 },
    { 460, -208 },
    { 392, -220 },
    { 488, -240 },
    { 328, -208 },
    { 440, -168 },
    { 420, -188 },
    { 432, -176 },
    { 240,  -16 },
    { 416, -192 },
    { 424, -160 },
    { 288,   -8 },
    { 436, -188 },
    { 224,   -1 },
    { 272,  -16 },
};


/* Sony XA ADPCM, defined for CD-DA/CD-i in the "Red Book" (private) or "Green Book" (public) specs.
 * The algorithm basically is BRR (Bit Rate Reduction) from the SNES SPC700, while the data layout is new.
 *
 * See end for accuracy information and layout info. */

typedef struct {
    uint8_t frame[0x80];
    int16_t* sbuf;
    int channels;
    int32_t hist1;
    int32_t hist2;
    int subframes;
    int is_xa8;
    int is_ea;
} xa_t;

static void decode_xa_frame(xa_t* xa, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i,j, samples_done = 0, sample_count = 0;
    int shift_max = xa->is_xa8 ? 8 : 12;
    int shift_limit = xa->is_xa8 ? 8 : 9; /* from Nocash PSX docs (in 8-bit mode max range should be 8 though) */

    /* decode subframes */
    for (i = 0; i < xa->subframes / xa->channels; i++) {
        uint8_t sp;
        int index, shift, sp_pos;
#ifdef XA_FLOAT
        float coef1, coef2;
#else
        int32_t coef1, coef2;
#endif

        /* parse current subframe (sound unit)'s header (sound parameters) */
        sp_pos = xa->is_xa8 ?
            i*xa->channels + channel :
            0x04 + i*xa->channels + channel;
        sp = xa->frame[sp_pos];
        index = (sp >> 4) & 0xf;
        shift = (sp >> 0) & 0xf;

        /* mastered values like 0xFF exist [Micro Machines (CDi), demo and release] */
        VGM_ASSERT_ONCE(shift > shift_max, "XA: incorrect shift %x\n", sp);
        if (shift > shift_max)
            shift = shift_limit;

        if (xa->is_ea) {
            coef1 = EA_TABLE[index][0];
            coef2 = EA_TABLE[index][1];
        }
        else {
            VGM_ASSERT_ONCE(index > 4, "XA: incorrect coefs %x\n", sp);
            coef1 = K0[index];
            coef2 = K1[index];
        }


        /* decode subframe nibbles */
        for(j = 0; j < 28; j++) {
            int32_t sample;
            uint8_t su;

            /* skip half decodes to make sure hist isn't touched (kinda hack-ish) */
            if (!(sample_count >= first_sample && samples_done < samples_to_do)) {
                sample_count++;
                continue;
            }

            if (xa->is_xa8) {
                int su_pos = (xa->channels==1) ?
                    0x10 + j*0x04 + i :             /* mono */
                    0x10 + j*0x04 + i*2 + channel;  /* stereo */

                su = xa->frame[su_pos];
                sample = (int16_t)((su << 8) & 0xff00) >> shift; /* 16b sign extend + scale */
            }
            else {
                int su_pos = (xa->channels==1) ?
                    0x10 + j*0x04 + (i/2) : /* mono */
                    0x10 + j*0x04 + i;      /* stereo */
                int get_high_nibble = (xa->channels==1) ?
                    (i&1) :         /* mono (even subframes = low, off subframes = high) */
                    (channel == 1); /* stereo (L channel / even subframes = low, R channel / odd subframes = high) */

                su = xa->frame[su_pos];
                su = get_high_nibble ?
                        (su >> 4) & 0x0f :
                        (su >> 0) & 0x0f;
                sample = (int16_t)((su << 12) & 0xf000) >> shift; /* 16b sign extend + scale */
            }

#if XA_FLOAT
            sample = sample + (coef1 * xa->hist1 + coef2 * xa->hist2);
#else
            if (xa->is_ea) /* sample << 8 actually but UB on negatives */
                sample = (sample * 256 + coef1 * xa->hist1 + coef2 * xa->hist2) >> 8;
            else
                sample = sample + ((coef1 * xa->hist1 + coef2 * xa->hist2 + 32) >> 6);
#endif

            xa->hist2 = xa->hist1;
            xa->hist1 = sample;
            sample = clamp16(sample); /* don't clamp hist */
            if (xa->is_ea)
                xa->hist1 = sample; /* do clamp hist */

            xa->sbuf[samples_done * xa->channels] = sample;
            samples_done++;

            sample_count++;
        }
    }
}


void decode_xa(VGMSTREAM* v, sample_t* outbuf, int32_t samples_to_do) {
    uint32_t offset = v->ch[0].offset; /* L/R share offsets */
    STREAMFILE* sf = v->ch[0].streamfile;

    int ch;
    int frames_in, bytes, samples_per_frame;
    uint32_t frame_offset, bytes_per_frame;
    int32_t first_sample = v->samples_into_block;
    xa_t xa;

    xa.channels = v->channels > 1 ? 2 : 1; /* only stereo/mono modes */
    xa.is_xa8 = (v->coding_type == coding_XA8);
    xa.is_ea = (v->coding_type == coding_XA_EA);
    xa.subframes = (xa.is_xa8) ? 4 : 8;

    /* external interleave (fixed size), mono/stereo */
    bytes_per_frame = sizeof(xa.frame);
    samples_per_frame = 28 * xa.subframes / v->channels;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = offset + bytes_per_frame * frames_in;
    bytes = read_streamfile(xa.frame, frame_offset, bytes_per_frame, sf);
    if (bytes != sizeof(xa.frame)) /* ignore EOF errors */
        memset(xa.frame + bytes, 0, bytes_per_frame - bytes);

    /* headers should repeat in pairs, except in EA's modified XA */
    VGM_ASSERT_ONCE(!xa.is_ea &&
            (get_u32be(xa.frame+0x0) != get_u32be(xa.frame+0x4) || get_u32be(xa.frame+0x8) != get_u32be(xa.frame+0xC)),
            "bad frames at %x\n", frame_offset);

    for (ch = 0; ch < xa.channels; ch++) {
        VGMSTREAMCHANNEL* stream = &v->ch[ch];

        xa.sbuf = outbuf+ch;
        xa.hist1 = stream->adpcm_history1_32;
        xa.hist2 = stream->adpcm_history2_32;

        decode_xa_frame(&xa, first_sample, samples_to_do, ch);

        stream->adpcm_history1_32 = xa.hist1;
        stream->adpcm_history2_32 = xa.hist2;
    }
}


size_t xa_bytes_to_samples(size_t bytes, int channels, int is_blocked, int is_form2, int bps) {
    int subframes = (bps == 8) ? 4 : 8;
    if (is_blocked) {
        return (bytes / 0x930) * (28*subframes/ channels) * (is_form2 ? 18 : 16);
    }
    else {
        return (bytes / 0x80) * (28*subframes / channels);
    }
}

/*
 * Official XA decoding is defined in diagrams (not implementation), roughly as:
 *     pcm = signed_nibble * 2^(12-range) + K0[index]*hist1 + K1[index]*hist2
 *  - K0 = 0.0, 0.9375, 1.796875, 1.53125 / K1 = 0.0, 0.0, -0.8125, -0.859375
 *    - int coef tables commonly use N = 6 or 8, so K0 0.9375*64 = 60 or 0.9375*256 = 240
 * - Range (12-range=shift) and filter index are renewed every ~28 samples.
 * - nibble is expanded to a signed 16b sample, reimplemented as:
 *     short sample = ((nibble << 12) & 0xf000) >> shift
 *     or: int sample = ((nibble << 28) & 0xf0000000) >> (shift + N)
 * - K0/K1 are float coefs are typically redefined with int math in various ways, with non-equivalent rounding:
 *     (sample + K0*2^N*hist1 + K1*2^N*hist2 + [(2^N)/2]) / 2^N
 *     (sample + K0*2^N*hist1 + K1*2^N*hist2 + [(2^N)/2]) >> N
 *     sample + (K0*2^N*hist1 + K1*2^N*hist2)>>N
 *     sample + (K0*2^N*hist1)>>N + (K1*2^N*hist2)>>N
 *     ...
 *   (rounding differences should be inaudible)
 *
 * There isn't an official implementation, but supposedly CD-ROM controller (which reads CD-XA) pushes
 * audio data to the SPU directly (http://wiki.psxdev.ru/index.php/SPU), so probably the same as PS-ADPCM.
 * Emus all seem to use approximations:
 * - duckstation: N=6, "s + ((h1 * K0) + (h2 * K1) + 32) / 64"; no hist clamp; s clamp;
 * - mednafen: N=6, "s + ((h1 * K0) >> 6) + ((h2 * K1) >> 6)"; hist / s clamp;
 * - mame: N=6, "s + ((h1 * K0) + (h2 * K1) + 32) >> 6)"; no hist / s clamp;
 * - peops: N=10, "s + ((h1 * K0) + (h2 * K1) + 32) >> 10)"; no hist / s clamp;
 * - pcsc: N=10, "s + ((h1 * K0) + (h2 * K1)) >> 10)"; hist / s clamp;
 * - cedimu: f32, "s + (h1 * k0 + h2 * k1)"; no hist / s clamp
 * It's not clear from the diagrams if hist should be clamped (seemingly not), but reportedly not
 * clamping improves output waveform spikes, while N=6 seems most common, and +32
 * (note <-0 / 64 != <0 >> 6)
 *
 * PS1 XA is apparently upsampled and interpolated to 44100, vgmstream doesn't simulate this.
 * Various XA descendants (PS-ADPCM, EA-XA, NGC DTK, FADPCM, etc) do filters/rounding slightly
 * differently too, maybe using one of the above methods in software/CPU, but in XA's case may
 * be done like the SNES/SPC700 BRR, with specific per-filter ops rather than a formula.
 *
 * XA has an 8-bit decoding and "emphasis" modes, that no PS1 game actually uses, but apparently
 * are supported by the CD hardware and will play if found. Some odd CD-i game does use 8-bit mode.
 * Official "sound quality level" modes:
 * - Level A: 37.8hz, 8-bit
 * - Level B: 37.8hz, 4-bit
 * - Level C: 18.9hz, 4-bit
 *
 * Info (Green Book): https://www.lscdweb.com/data/downloadables/2/8/cdi_may94_r2.pdf
 * BRR info (no$sns): http://problemkaputt.de/fullsnes.htm#snesapudspbrrsamples
 *           (bsnes): https://github.com/byuu/bsnes/blob/master/bsnes/sfc/dsp/SPC_DSP.cpp#L316
 * Note that this is just called "ADPCM" in the "CD-ROM XA" spec (rather than "XA ADPCM" being the actual name).
 */ 

/* data layout (mono):
 * - CD XA audio is divided into sectors ("audio blocks"), each with 18*0x80 frames
 *   (sectors are handled externally, this decoder only sees 0x80 frames)
 * - each frame ("sound group") is divided into 8 interleaved subframes ("sound unit"), with
 *   8*0x01 subframe headers x2 ("sound parameters") first then 28*0x04 subframe nibbles ("sound data")
 * - subframe headers: 0..3 + repeat 0..3 + 4..7 + repeat 4..7 (where N = subframe N header)
 *   (repeats may be for error correction, though probably unused)
 *   header has a "range" N (gain of 2^N, or simplified as a shift) and a "filter" (control gains K0 and K1)
 * - subframe nibbles: 32b with nibble0 for subframes 0..8, 32b with nibble1 for subframes 0..8, etc
 *   (low first: 32b = sf1-n0 sf0-n0  sf3-n0 sf2-n0  sf5-n0 sf4-n0  sf7-n0 sf6-n0, etc)
 *
 * stereo layout is the same but alternates channels: subframe 0/2/4/6=L, subframe 1/3/5/7=R
 *
 * example:
 *   2A29282A 2A29282A 29292929 29292929
 *   0D0D20FB 0D8B011C 0EFA103B 5FEC2F42
 *   ...
 * 
 *   subframe 0: header @ 0x00 or 0x04, 28 nibbles (low)  @ 0x10,14,18,1c,20 ... 7c
 *   subframe 1: header @ 0x01 or 0x05, 28 nibbles (high) @ 0x10,14,18,1c,20 ... 7c
 *   subframe 2: header @ 0x02 or 0x06, 28 nibbles (low)  @ 0x11,15,19,1d,21 ... 7d
 *   ...
 *   subframe 7: header @ 0x0b or 0x0f, 28 nibbles (high) @ 0x13,17,1b,1f,23 ... 7f
 *
 * 8-bit layout is similar but only has 4 subframes + subframe bytes, so half the samples:
 *   subframe 0: header @ 0x00 or 0x04/08/0c, 28 bytes    @ 0x10,14,18,1c,20 ... 7c
 *   subframe 1: header @ 0x01 or 0x05/09/0d, 28 bytes    @ 0x11,16,19,1d,21 ... 7d
 *   ...
 *   subframe 3: header @ 0x03 or 0x07/0b/0f, 28 bytes    @ 0x13,17,1b,1f,23 ... 7f
 * 
 * XA-EA found in EA SAT games set subframes header like: 0..3 null + 0..3 ok + 4..7 ok + 4..7 null
 * so decoder only reads those.
 */
