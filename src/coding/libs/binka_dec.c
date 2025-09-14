/* This is mainly based on decompilations of binkawin.asi/radutil.dll. Some parts could be simplified/optimized
 * a bit but mostly follows the original style for doc purposes.
 *
 * It supports Unreal Engine's binka variation (Binka Audio 2), though that implementation is slightly different and
 * more optimized. Note that Binka src is apparently available in UE5, but this code doesn't use anything from it,
 * and was reversed from scrath.
 *
 * Original audio packets from bink videos probably work too but weren't tested (old RDFT mode).
 *
 * Binka decoders were designed as mono or stereo (possibly due to how they handle streams in bink videos),
 * so they use N separate decoders for multichannel. OG lib allocs decoder + bufs (mono/stereo) per each one;
 * we handle it a bit differently and could be simplified but kept anyway.
 */
//TODO: test RDFT stereo (uses frame_channels 1)
//TODO: check Bink Audio 1.0 (~1999 games, supposedly has bitstream diffs but ~2001 libs only handle 1.1+)

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "../../util/bitstream_lsb.h"
#include "binka_dec.h"
#include "binka_data.h"
#include "binka_transform.h"

#define MAX_DECODERS 8
#define MAX_FRAME_CHANNELS 2
#define MAX_CHANNELS 8              // OG lib seems like it could handle 2ch * 8 decoders = 16, but encoder only allows up to 7.1
#define MIN_CHANNELS 1
#define MAX_SAMPLE_RATE 96000       // OG lib/encoder doesn't check this (BCF1 uses 16-bit field, UEBA has ~96000hz files)
#define MIN_SAMPLE_RATE 300
#define MAX_FRAME_SAMPLES  2048
#define MAX_FRAME_OVERLAP  128
#define MAX_BANDS 26

#define FLAGS_DCT               (1<<0)  // Original Binka uses RDFT (removed in Binka 2.0)
#define FLAGS_OUTPUT_PLANAR     (1<<1)  // how to reorder resulting samples (doc only, this decoder ignores it)
#define FLAGS_BINKA2            (1<<2)  // "Bink Audio 2.0", prev was "Bink Audio 1.1" (seen in the 2021.06 changelog)
// internal flags not part of original lib
//#define FLAGS_EXTRA_BINKA1    (1<<7)  // "Bink Audio 1.0" has small diffs


typedef struct {
    //void* overlap_buf;        //00 (pointer to alloc'd buf after decoder end)
    const float* table;         //04
    int frame_samples;          //08
    float scale;                //0c
    //int pcm_size;             //10
    int overlap_samples;        //14 (pcm size in OG lib)
    int overlap_bits;           //18
    int frame_channels;         //1C
    int is_first_frame;         //20
    int band_count;             //24
    int transform_size;         //28 (see 04)
    uint32_t flags;             //2c
    //uint32_t alloc_size;      //30
    int band_thresholds[MAX_BANDS]; //34
    // 38+: alloc_size'd bufs
}  binka_decoder_t;


static void open_decoder(binka_decoder_t* decoder, int sample_rate, int frame_channels, uint32_t flags) {
    int frame_samples;
    if (sample_rate < 22050)
        frame_samples = 512;
    else if (sample_rate < 44100)
        frame_samples = 1024;
    else
        frame_samples = 2048;

    if ((flags & FLAGS_DCT) == 0) {
        // Interleaved mode (mixed channel data, used for RDFT only?)
        // UE Binka removes RDFT and related checks like this one.
        sample_rate *= frame_channels;
        frame_samples *= frame_channels;
        frame_channels = 1; //only for calcs below
    }

    int frame_samples_half = frame_samples >> 1;
    int sample_rate_half = (sample_rate + 1) >> 1;

    int band_count = 0;
    while (band_count < (MAX_BANDS - 1)) {
        if (binka_cutoff_frequency[band_count] >= sample_rate_half)
            break;
        band_count++;
    }

    memset(decoder, 0, sizeof(binka_decoder_t));

    if (frame_channels == 1) {
        flags &= 0xFFFFFFFD;  //remove FLAGS_OUTPUT_PLANAR
    }

    //int pcm_size = 2 * frame_channels * frame_samples;
    //decoder->overlap_buf = &decoder->end;
    //decoder->alloc_size = ((pcm_size >> 4) + 0xAF) & 0xFFFFFFF0;
    decoder->table = binka_cosines; // remnant from early radutil versions, where it was calculated on init 
    decoder->transform_size = 2048; // implicit in UE
    decoder->flags = flags;
    decoder->overlap_samples = frame_samples >> 4;
    decoder->frame_channels = frame_channels;
    decoder->band_count = band_count;
    decoder->frame_samples = frame_samples;
    //decoder->pcm_size = pcm_size;

    decoder->overlap_bits = 0;
    switch (frame_samples >> 4) {
        case 32:
            decoder->overlap_bits = 5;
            break;
        case 64:
            decoder->overlap_bits = 6;
            break;
        case 128:
            decoder->overlap_bits = 7;
            break;
        case 256: //RDFT stereo only
            decoder->overlap_bits = 8;
            break;
        default: //not part of allowed frame_samples
            break;
    }

    // for orthogonal(?) normalization
    // UE Binka (that only has DCT mode) uses equivalent constants when loading frame sizes:
    // 512=0.044194173, 1024=0.0625, 2048=0.088388346. Some bink versions use sqrtf (shouldn't matter).
    decoder->scale = 2.0 / sqrt(frame_samples);

    // calculate points where bands change
    // (these values are actually * 2 during unpack, could be pre-init'd instead)
    for (int i = 0; i < band_count; i++) {
        int band_limit = frame_samples_half * binka_cutoff_frequency[i] / sample_rate_half;
        if (band_limit == 0) // i = 0 only
            band_limit = 1;
        decoder->band_thresholds[i] = band_limit;
    }
    decoder->band_thresholds[band_count] = frame_samples_half;

    // setup flag for overlap
    decoder->is_first_frame = true;
}

// Apply overlap of first samples with prev last samples.
// OG does it over pcm and has regular and SIMD functions that are used depending on CPU flags
// (there seems more rounding diffs than usual at the beginning of each frame due to this).
// Binka doesn't have encoder delay/skip samples (not a lapped transform) but reserves last samples
// of a frame to 'window' with next frame, maybe to improve frame transitions.
static void apply_overlap(float* dst, int overlap_samples, float* overlap, int overlap_bits) {
    if (!overlap_bits)
        return;

    for (int i = 0; i < overlap_samples; i++) {
        float s1 = overlap[i];
        float s2 = (i * (dst[i] - s1)) / overlap_samples; // >> overlap_bits; //shift in pcm only;
        dst[i] = (s1 + s2);
    }
}

static void apply_scale(float* coefs, float scale, int frame_samples) {
    //scale = scale / 32767.0; // output is pcm-like
    for (int i = 0; i < frame_samples; i++) {
        coefs[i] *= scale;
    }
}

/* Outputs final samples, in planar (reinterleaved later) format and floats.
 *
 * OG lib re-interleaves and decodes to pcm16 (clamps too) in separate functions optimized depending on CPU flags.
 * Overlap is done with pcm16 too. UE Binka seems to integrate it as part of transform steps though.
 */
static void output_samples(binka_decoder_t* dec, float* coefs, float* overlap) {

    apply_scale(coefs, dec->scale, dec->frame_samples * dec->frame_channels);

    int output_samples = dec->frame_samples - dec->overlap_samples;

    int current_overlap_bits = dec->overlap_bits;
    if (dec->is_first_frame) { // done externally to decode in OG lib
        dec->is_first_frame = false;
        current_overlap_bits = 0;
    }

    float* coefs_ch = coefs;
    float* overlap_ch = overlap;
    for (int ch = 0; ch < dec->frame_channels; ch++) {
        apply_overlap(coefs_ch, dec->overlap_samples, overlap_ch, current_overlap_bits);

        // memmove in OG lib, though not sure if they actually share bufs
        memcpy(overlap_ch, &coefs_ch[output_samples], dec->overlap_samples * sizeof(float));

        coefs_ch += dec->frame_samples;
        overlap_ch += dec->overlap_samples;
    }

#if 0
    // OG lib, roughly
    if (dec->flags & FLAGS_OUTPUT_PLANAR) {
        scale_mono(dst, coefs, scale, all_frame_samples);
        if (overlap_bits) {
            overlap_samples(dst, overlap_size, overlap, overlap_bits);
            overlap_samples(dst + N, overlap_size, overlap + N, overlap_bits);
        }
    }
    else {
        if (dec->frame_channels == 1)
            scale_mono(dst, coefs, scale, frame_samples);
        else
            scale_interleave_stereo(dst, coefs, scale, frame_samples);
        if (overlap_bits)
            overlap_samples(dst, overlap_size, overlap, overlap_bits);
    }
#endif
}


// Unpack Bink Audio's 29-bit floats
// Incidentally their bitreader uses LSB style because they read 32-bit LE ints
static float get_float29(bitstream_t* is) {
    uint32_t code = bl_read(is, 29);
    int power = (code >> 0) & 0x1f;
    int mantissa = (code >> 5) & 0x77FFFFF; //odd mask but upper bits aren't set anyway
    int sign =  (code >> 28) & 0x01;

    float value = (float)(mantissa * binka_float29_power[power]);
    if (sign)
        value = -value;
    return value;
}

// Unpack coefs for a single channel
static bool unpack_channel(float* coefs, int frame_samples, int band_count, bitstream_t* is, const int* band_thresholds, bool is_binka2) {

#if 0
    // binka1 validates available max during reads while binka2 does it at first
    if (is_binka2 && not_enough_data) {
        memset(...)
        return false;
    } 
#endif

    // read first 2 coefs as packed floats
    coefs[0] = get_float29(is);
    coefs[1] = get_float29(is);

    // read base scales per band, first and last seem fixed (and not used?)
    int i_bits = is_binka2 ? 7 : 8;
    float scalefactors[MAX_BANDS];
    for (int i = 0; i < band_count; i++) {
        int index = bl_read(is, i_bits);
        if (index > 95)
            index = 95;
        scalefactors[i] = binka_scalefactors[index];
    }

    // setup initial band
    float band_scalefactor = 0.0;
    int band = 0;
#if 0
    // found in older versions (removed when Binka2 was added), maybe a remnant as
    // band_threshold[0] = 1 * 2 >= 2, and band/scale is updated below first
    for (band = 0; band_thresholds[band] * 2 < 2; band++) {
        band_scalefactor = scalefactors[band];
    }
#endif

    // read codes for all bands
    int pos = 2; // after first two coefs (band 1)
    while (pos < frame_samples) {
        int end;

        // calculate next batch of coefs
        // (8 * N depending on RLE table, all sharing the same number of q_bits)
        int rle_flag = bl_read(is, 1);
        if (rle_flag) {
            int rle_index = bl_read(is, 4);
            end = pos + 8 * binka_rle_table[rle_index];
        }
        else {
            end = pos + 8;
        }
        if (end > frame_samples)
            end = frame_samples;

        // get size in the bitstream of coefs
        int q_bits = bl_read(is, 4);
        if (q_bits > 0) {

            // read batch of N coefs
            if (is_binka2) {
                // Binka2 packs coefs then signs, using vectorized/unrolled functions (fiddling with bitreader positions),
                // then applies scales in next loop via SIMDs to optimize performance a bit (considering coefs are read in batches of 8).

                // read coefs
                for (int subpos = pos; subpos < end; subpos++) {
                    int value = bl_read(is, q_bits);
                    coefs[subpos] = value;
                    // could read sign here with a temp bitreader like OG lib to minimize loops
                }

                // read signs
                for (int subpos = pos; subpos < end; subpos++) {
                    if (coefs[subpos] != 0.0) {
                        int negative = bl_read(is, 1);
                        if (negative)
                            coefs[subpos] = -coefs[subpos];
                    }
                }

                // scale
                while (pos < end) {
                    // move band after reaching threshold
                    if (pos == band_thresholds[band] * 2) {
                        band_scalefactor = scalefactors[band];
                        band++;
                    }

                    coefs[pos] *= band_scalefactor;
                    pos++;
                }
            }
            else {
                // read and scale coefs
                while (pos < end) {
                    // move band after reaching threshold
                    if (pos == band_thresholds[band] * 2) {
                        band_scalefactor = scalefactors[band];
                        band++;
                    }

                    // new coef
                    int value = bl_read(is, q_bits);
                    if (value) {
                        int negative = bl_read(is, 1);

                        float coef = value * band_scalefactor;
                        if (negative)
                            coef = -coef;
                        coefs[pos] = coef;
                    }
                    else {
                        coefs[pos] = 0.0f;
                    }
                    pos++;
                }
            }
        }
        else {
            // coefs not saved in the bitstream, set batch to 0
            memset(coefs + pos, 0, (end - pos) * sizeof(float)); // UE Binka has for ... coef[i] = 0
            pos = end;

            // move band after reaching threshold
            while (end > band_thresholds[band] * 2) {
                band_scalefactor = scalefactors[band];
                band++;
            }
        }
    }

#if 0
    // TODO: recheck meaning
    if (is_binka2) {
        //int padding_size = bl_read(is, 6);
        //bl_skip(is, 64 - padding_size);
    }
#endif

    if (is->error)
        return false;
    return true;
}

static int decode_frame(binka_decoder_t* dec, unsigned char* src, int src_size, float* coefs, float* overlap) {
    bitstream_t is_tmp;
    bitstream_t* is = &is_tmp;
    bl_setup(is, src, src_size);

    bool is_dct = dec->flags & FLAGS_DCT;
    bool is_binka2 = dec->flags & FLAGS_BINKA2; // OG lib uses two separate functions for binka1/2 unpack


    if (is_dct) {
        bl_skip(is, 2); // OG lib simply skips this (always 0, reserved?)
    }

    float* coefs_tmp = coefs;
    for (int ch = 0; ch < dec->frame_channels; ch++) {

        // decode coefs for each channel
        bool ok = unpack_channel(coefs_tmp, dec->frame_samples, dec->band_count, is, dec->band_thresholds, is_binka2);
        if (ok) {
            if (is_dct) {
                transform_dct(coefs_tmp, dec->frame_samples, dec->transform_size, dec->table);
            }
            else {
                transform_rdft(coefs_tmp, dec->frame_samples, dec->transform_size, dec->table);
            }
        }
        else {
            // OG lib keeps going but shouldn't happen (memset was integrated in unpack with Binka2)
            //memset(coefs, 0, dec->frame_samples * sizeof(float));
            return -1;
        }

        coefs_tmp += dec->frame_samples;
    }

    output_samples(dec, coefs, overlap);


    // OG lib checks the bitreader's initial *src vs current *src (padded).
    // in multichannel there may be more data beyond (aligned).
    bl_align(is, 32);
    int bytes_done = is->b_off / 8;
    return bytes_done;
}


//-----------------------------------------------------------------------------
// API (not quite like the original code)

struct binka_handle_t {
    // config
    int channels;
    int sample_rate;
    int output_samples;

    // state
    binka_decoder_t decoders[MAX_DECODERS];
    float* samples;
    float* overlap;

    int decoder_count;
};

binka_handle_t* binka_init(int sample_rate, int channels, binka_mode_t mode) {
    uint32_t flags;
    int temp_channels;

    if (sample_rate < MIN_SAMPLE_RATE || sample_rate > MAX_SAMPLE_RATE)
        return NULL;
    if (channels < MIN_CHANNELS || channels > MAX_CHANNELS)
        return NULL;

    binka_handle_t* ctx = calloc(1, sizeof(binka_handle_t));
    if (!ctx) goto fail;

    ctx->samples = calloc(1, MAX_FRAME_SAMPLES * channels * sizeof(float));
    if (!ctx) goto fail;

    ctx->overlap = calloc(1, MAX_FRAME_OVERLAP * channels * sizeof(float));
    if (!ctx) goto fail;

    ctx->channels = channels;
    ctx->sample_rate = sample_rate;
    ctx->decoder_count = (channels + 1) / MAX_FRAME_CHANNELS;

    // base frame samples without overlap
    if (ctx->sample_rate < 22050)
        ctx->output_samples = 480;
    else if (ctx->sample_rate < 44100)
        ctx->output_samples = 960;
    else
        ctx->output_samples = 1920;

    flags = 0x00;
    switch (mode) {
        case BINKA_BFC:
            if (channels > 2)
                flags |= FLAGS_OUTPUT_PLANAR;
            flags |= FLAGS_DCT;
            break;

        case BINKA_UEBA: {
            // In the 'UEBA open' method these flags are always set, maybe reserved
            bool ueba_flag1 = true;
            bool ueba_flag2 = true;

            if (!ueba_flag1)
                flags |= FLAGS_OUTPUT_PLANAR;
            if (ueba_flag2)
                flags |= FLAGS_BINKA2;

            //extra flags (not set but implicit in UEBA)
            flags |= FLAGS_DCT;
            break;
        }

        default:
            goto fail;
    }

    // all decoders use 2ch except last
    temp_channels = channels;
    for (int i = 0; i < ctx->decoder_count; i++) {
        int frame_channels = temp_channels >= 2 ? 2 : 1;
        open_decoder(&ctx->decoders[i], sample_rate, frame_channels, flags);
        temp_channels -= 2;
    }

    binka_reset(ctx);

    return ctx;
fail:
    binka_free(ctx);
    return NULL;
}

/* copy buffer in 11...22...33...44 format... into 12341234... (src has more samples than dst buf) */
static void copy_samples(float* dst, float* src, int src_samples, int dst_samples, int channels) {
    for (int ch = 0; ch < channels; ch++) {
        int dst_i = ch;
        int src_i = src_samples * ch;
        for (int s = 0; s < dst_samples; s++) {
            dst[dst_i] = src[src_i];
            dst_i += channels;
            src_i++;
        }
    }
}

int binka_decode(binka_handle_t* ctx, unsigned char* src, int src_size, float* dst) {
    if (!ctx)
        return -1;

    // 1 packet has N mono/stereo frames
    float* samples = ctx->samples;
    float* overlap = ctx->overlap;
    for (int i = 0; i < ctx->decoder_count; i++) {
        binka_decoder_t* decoder = &ctx->decoders[i];

        int bytes_done = decode_frame(decoder, src, src_size, samples, overlap);
        if (bytes_done < 0)
            return bytes_done;

        samples += decoder->frame_samples * decoder->frame_channels;
        overlap += decoder->overlap_samples * decoder->frame_channels;
        src += bytes_done;
        src_size -= bytes_done;
    }

    // packets seem aligned to 0x04, more is probably our bug
    if (src_size > 0x04)
        return -src_size;

    copy_samples(dst, ctx->samples, ctx->decoders[0].frame_samples, ctx->output_samples, ctx->channels);

    return ctx->output_samples;
}


void binka_free(binka_handle_t* ctx) {
    if (!ctx)
        return;

    free(ctx->samples);
    free(ctx->overlap);
    free(ctx);
}

void binka_reset(binka_handle_t* ctx) {
    if (!ctx)
        return;

    for (int i = 0; i < ctx->decoder_count; i++) {
        ctx->decoders[i].is_first_frame = true;
    }

    // no need to reset overlaps/etc
}

int binka_get_frame_samples(binka_handle_t* ctx) {
    if (!ctx)
        return 0;
    return ctx->output_samples;
}
