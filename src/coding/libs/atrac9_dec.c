#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "atrac9_dec.h"
#include "atrac9_internal.h"
#include "../../util/bitstream_msb.h"

/* Reversed from libSceSnds (v1.07, with debug symbols) and Sony's libs, with much help from Thealexbarney's LibAtrac9/decomps:
 * - https://github.com/Thealexbarney/LibAtrac9
 * - https://github.com/Thealexbarney/VGAudio/tree/master/docs/audio-formats/atrac9
 *
 * This lib follows libSceSnds style/naming (also based on LDAC), since it was actually used in games. Sony's PC libs
 * seems to be general purpose and unlikely to be used by real-time games: different API (encode + decode), double bufs,
 * more memory (unpacks all blocks before transforming), more generic iMDCT... The end results are exactly the same with
 * minor +-1 diffs, though. This lib also simplifies and moves around some things as well.
 *
 * Known ATRAC9 versions (minor diffs marked in code):
 * - v1.00 (~2009): original
 * - v1.07 (~2015): band extension, multichannel (4/6/8), wide band (encode only), LFE 'super low cut' (encode only)
 * - v2.00 (~2016): internal changes?
 * - v3.03 (~2020): vibration (LQ) modes, DMC (Discrete MultiChannel) up to 64ch, more bitrates (encode only)
 */

/* Some extra bits about ATRAC9:
 *
 * ATRAC9 features are selected with a 32-bit "config data" value, read from headers and passed to the decoder.
 * Data is made of CBR "superframes" containing 1/2/4 VBR "frames" + padding, internally of 1-5 "blocks" of 64/128/256
 * samples. libs decode frame by frame, so caller is meant to read 1 superframe and decode N frames manually
 * (decoded frame size is returned).
 *
 * Blocks have data for 1/2 channels, so the number of blocks per frame depends on channel config (like 2ch = 1 dual
 * stereo block or 2 mono blocks). Frames are a logical grouping, and the bitstream only has block data. Presumably
 * the encoder puts more frames in a superframe to improve bitrate, since some features can be reused between frames/blocks.
 * - 5.1 example: superframe = [ frame 0 | frame 1 + padding ] -> frame = [ block 2ch | block 1ch | block LFE | block 2ch ]
 *
 * V3 introduces a "DMC" mode that uses decoder-level interleaved superframes, probably to keep it frame-based.
 * Every last frame also has padding, maybe for simpler bitrate control or reusing existing padding handling.
 * - 2.0 example: superframe = [ ch0 frame 0 | ch1 frame 0 | ch0 frame 1 + padding | ch1 frame 1 + padding ]
 *
 * Decoding one block works like this:
 * - read block config bits
 * - read band info + count
 * - read gradient (bitalloc tuning) info
 * - read extra parameters (stereo, band extension)
 * - (per channel)
 *   - read scalefactors indexes (of various encoding modes)
 *   - determine bit allocation (word lengths) from scalefactors + gradient ('noise shaping')
 *   - determine huffman codebooks ('detect projection')
 *   - read 'huffman' spectral coefs (coarse)
 *   - read 'component' spectral coefs (fine)
 * - handle intensity stereo coefs
 * - denormalize coefs (apply scalefactors)
 * - handle block padding
 * - apply band extension (create extra spectrum mirror/noise QUs)
 * - transform to samples with IMDCT + window + overlap
 * - output samples
 * - validate superframe padding (after all frames)
 *
 * The number of quantization unit (QU) coefs depends on band and block features (stereo/normal/band extension/etc).
 * Bitsizes depend on scalefactor and a gradient curve, so louder(?) coefs get more bits. Blocks can be LFE/vibration,
 * where unpacking is simplified. Some features/config may be reused from prev frame/blocks too.
 *
 * In OG libs many helper functions aren't static/inlined, maybe fragmented like Sony's LDAC (related to ATRAC9):
 * - https://android.googlesource.com/platform/external/libldac/+/eeee1a3f5f8df1282e3a6d297085885fd886737b/src/
 */

//-----------------------------------------------------------------------------
// DECODE DATA: CONFIG

// OG: _ChanConfig
typedef struct {
    uint8_t channels;
    uint8_t block_count;
    uint8_t block_types[8];
} at9_channel_config_t;

// OG: ga_chan_config_tbl
static const at9_channel_config_t channel_config_table[8] = {
    { 1,  1, { AT9_BLOCK_MONO } }, // 1.0: C
    { 2,  2, { AT9_BLOCK_MONO, AT9_BLOCK_MONO } }, // 2.0: L R (dual mono)
    { 2,  1, { AT9_BLOCK_STEREO } }, // 2.0: L-R
    { 6,  4, { AT9_BLOCK_STEREO, AT9_BLOCK_MONO, AT9_BLOCK_LFE, AT9_BLOCK_STEREO } }, // 5.1: L-R C LFE SL-SR
    { 8,  5, { AT9_BLOCK_STEREO, AT9_BLOCK_MONO, AT9_BLOCK_LFE, AT9_BLOCK_STEREO, AT9_BLOCK_STEREO } }, // 7.1: L-R C LFE SL-SR BL-BR
    { 4,  2, { AT9_BLOCK_STEREO, AT9_BLOCK_STEREO } }, // 4.0: L-R SL-SR
    // V3 addition (prev versions have 0s)
    { 1,  1, { AT9_BLOCK_VIBRATION } }, // 1.0 vibration
    { 2,  2, { AT9_BLOCK_VIBRATION, AT9_BLOCK_VIBRATION } }, // 2.0 vibration (dual mono)
};

// OG: ga_at9_frame_samples
static const uint16_t frame_samples_table[16] = {
    64, 64, 128, 128, 128, 256, 256, 256,
    64, 64, 128, 128, 128, 256, 256, 256
};

/* >7 entries don't exist in libSceSnds, and it only allows 1/4/7 (12000/24000/48000). Encoders also only make
 * those rates, plus no other sample rates seem to exist in the wild. Code paths for other modes look correct tho. */
// OG: CSWTCH_19 (0..7 only)
static const int sample_rate_table[16] = {
    11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
    44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000
};

//-----------------------------------------------------------------------------

struct atrac9_handle_t {
    // config
    int sample_rate;
    int channel_count;                                          // total output channels
    int superframe_size;                                        // total size
    int stream_count;
    int dmc_channels;

    // state
    at9_core_t* cores;                                          // frame + block state
    at9_channel_t* channels;                                    // channel state

    at9_block_temp_t block_temp;                                // (OG: SceAt9DecTmpWork.abTmp)
    at9_channel_temp_t channel_temps[AT9_MAX_BLOCK_CHANNELS];   // (OG: SceAt9DecTmpWork.acTmp)
};


// OG: get_block_nchan
static int get_block_channels(int block_type) {
    switch (block_type) {
        case AT9_BLOCK_MONO:
        case AT9_BLOCK_LFE:
        case AT9_BLOCK_VIBRATION:
            return 1;
        case AT9_BLOCK_STEREO:
            return 2;
        default:
            // OG returns -1 yet isn't validated, which feels kind of dangerous (but never happens)
            return 0;
    }
}

// OG: outputMode (not an enum)
enum {
    AT9_OUTPUT_PCM16,       // 0x10010
    AT9_OUTPUT_FLOAT,       // 0x10120
    AT9_OUTPUT_FLOAT16,     // extra
};

/* write one block's samples into the interleaved dst buffer */
// OG: set_output_pcm_at9
static void output_samples(at9_channel_temp_t* channel_temps, void* dst, int output_mode, int channel_start, int channels, int block_channels, int samples) {
    if (output_mode == AT9_OUTPUT_PCM16) {
        int16_t* dst_pcm = dst;

        for (int ch = 0; ch < block_channels; ch++) {
            const float* src = channel_temps[ch].samples;

            for (int i = 0; i < samples; i++) {
                int sample = (int)floor((double)src[i] + 0.5);
                if (sample > 32767)
                    sample = 32767;
                else if (sample < -32768)
                    sample = -32768;
                dst_pcm[i * channels + channel_start + ch] = (int16_t)sample;
            }
        }
    }
    else if (output_mode == AT9_OUTPUT_FLOAT) {
        float* dst_flt = dst;

        for (int ch = 0; ch < block_channels; ch++) {
            const float* src = channel_temps[ch].samples;

            for (int i = 0; i < samples; i++) {
                float sample = src[i] * (1.0f / 32768.0f); // OG: 0.000030517578f
                if (sample > 1.0f)
                    sample = 1.0f;
                else if (sample < -1.0f)
                    sample = -1.0f;
                dst_flt[i * channels + channel_start + ch] = sample;
            }
        }
    }
    else if (output_mode == AT9_OUTPUT_FLOAT16) {
        float* dst_f16 = dst;

        for (int ch = 0; ch < block_channels; ch++) {
            const float* src = channel_temps[ch].samples;

            for (int i = 0; i < samples; i++) {
                float sample = src[i];
                dst_f16[i * channels + channel_start + ch] = sample;
            }
        }
    }
}

// OG: at9_decode_frame
static int decode_frame(at9_core_t* core, at9_block_temp_t* block_temp, at9_channel_temp_t* channel_temps,
        const uint8_t* src, int src_size, void* dst, int output_mode, int channel_start, int channel_count, int* p_bytes_used) {
    int res = AT9_OK;
    bitstream_t is = {0};

    int max_size = core->superframe_size - core->superframe_bytes_used;
    if (max_size > src_size)
        max_size = src_size;
    bm_setup(&is, (uint8_t*)src, max_size); //TODO pass const

    for (int i = 0; i < core->block_count; i++) {
        at9_block_t* block = &core->blocks[i];

        res = unpack_block(core, &is, block_temp, block, channel_temps);
        if (res < 0)
            return res;

        // OG overreads only check bitloc > 0x800*8 (superframe max), so presumably buf_size is meant to be fixed
        if (is.error)
            return AT9_ERROR_OVERREAD;

        res = transform_block(core, block, channel_temps);
        if (res < 0)
            return res;

        output_samples(channel_temps, dst, output_mode, channel_start, channel_count, block->channel_count, core->frame_samples);
        channel_start += block->channel_count;
    }

    // last frame in a superframe automatically reads validates + padding (only matters for DMC)
    if (core->frames_per_superframe == core->current_frame + 1) {
        res = unpack_superframe_padding(&is, core->superframe_size - core->superframe_bytes_used);
        if (res < 0)
            return res;
    }

    // update output info
    int bytes_used = bm_pos(&is) / 8;
    *p_bytes_used = bytes_used;
    core->superframe_bytes_used += bytes_used;
    core->current_frame++;
    if (core->current_frame == core->frames_per_superframe) {
        // useful?
        core->current_frame = 0;
        core->superframe_bytes_used = 0;
    }

    return res;
}


typedef struct {
    uint8_t sync;
    uint8_t profile_index;
    uint8_t channel_config_index;
    int frame_size;
    uint8_t data_version;
    uint8_t superframe_index;
    uint8_t reserved;
    int dmc_channels;
} at9_config_info_t;

// OG: unpack_config_data_at9 (w/o validations)
static int parse_config_data(const uint8_t* config_data, at9_config_info_t* info) {
    bitstream_t is;
    bm_setup(&is, (uint8_t*)config_data, ATRAC9_CONFIG_SIZE); //TODO pass const

    info->sync = bm_read(&is, 8);
    if (info->sync == 0xFE) {
        // standard ATRAC9 config
        info->profile_index = bm_read(&is, 4);
        info->channel_config_index = bm_read(&is, 3);
        info->data_version = bm_read(&is, 1);
        info->frame_size = bm_read(&is, 11) + 1;
        info->superframe_index = bm_read(&is, 2);
        info->reserved = bm_read(&is, 3);

        info->dmc_channels = 0;
    }
    else if (info->sync == 0x30) {
        // DMC header config
        info->profile_index = bm_read(&is, 4);
        info->dmc_channels = bm_read(&is, 6) + 1;
        info->data_version = bm_read(&is, 1);
        info->frame_size = bm_read(&is, 11) + 1;
        info->superframe_index = bm_read(&is, 2);

        info->channel_config_index = 0; // mono
        info->reserved = 0;
    }
    else {
        return AT9_ERROR_PARAMS;
    }

    // see init
    //if (info->profile_index > 15)
    //    return AT9_ERROR_PARAMS;

    // OG: at9_get_channel (also validates vs a external nChannels)
    //if (info->channel_config_index > 7)
    //    return AT9_ERROR_PARAMS;

    //at9_set_config_info
    if (info->superframe_index > 2)
        return AT9_ERROR_PARAMS;

    //TODO: check min? (not in OG and should work but odd)
    if (info->frame_size > AT9_MAX_FRAME_SIZE) // known max
        return AT9_ERROR_PARAMS;

    if (info->data_version != 0)
        return AT9_ERROR_PARAMS;

    // ignored in OG, use to detect new variations
    if (info->reserved != 0)
        return AT9_ERROR_PARAMS;

    return AT9_OK;
}

//-----------------------------------------------------------------------------
// API

/* init block config that gets overwritten on every frame */
// OG: at9_init_decode / init_at9syn (sans huffman tables, to use this as reset)
static void init_state(atrac9_handle_t* ctx) {
    memset(ctx->channels, 0, ctx->channel_count * sizeof(at9_channel_t));

    // probably not everything needs to be reset and could be simplified but...
    int channel = 0;
    for (int s = 0; s < ctx->stream_count; s++) {
        at9_core_t* core = &ctx->cores[s];
        const at9_channel_config_t* config = &channel_config_table[core->channel_config_index];

        memset(core->blocks, 0, sizeof(core->blocks));

        core->block_count = config->block_count;
        core->channel_count = config->channels;

        for (int i = 0; i < config->block_count; i++) {
            at9_block_t* block = &core->blocks[i];

            block->type = config->block_types[i];

            // OG checks get_block_channels every time
            block->channel_count = get_block_channels(block->type);

            for (int ch = 0; ch < block->channel_count; ch++) {
                block->channels[ch] = &ctx->channels[channel];
                channel++;
            }
        }

        core->current_frame = 0;
        core->superframe_bytes_used = 0;
    }
}

// OG: sceAt9DecCreateDecoder
atrac9_handle_t* atrac9_init(const uint8_t* config_data) {
    at9_config_info_t info = {0};
    atrac9_handle_t* ctx = NULL;

    if (!config_data)
        return NULL;

    int res = parse_config_data(config_data, &info);
    if (res < 0)
        return NULL;

    // only allowed indexes (see sample_rate_table)
    if (!(info.profile_index == 1 || info.profile_index == 4 || info.profile_index == 7))
        return NULL;

    ctx = calloc(1, sizeof(atrac9_handle_t));
    if (!ctx) goto fail;

    ctx->sample_rate = sample_rate_table[info.profile_index];

    if (info.dmc_channels) {
        ctx->stream_count = info.dmc_channels;
        ctx->channel_count = info.dmc_channels;
        if (ctx->channel_count <= 0 || ctx->channel_count > AT9_MAX_CHANNELS_DMC)
            goto fail;
    }
    else {
        ctx->stream_count = 1;
        ctx->channel_count = channel_config_table[info.channel_config_index].channels;
        if (ctx->channel_count <= 0 || ctx->channel_count > AT9_MAX_CHANNELS_STD)
            goto fail;
    }

    ctx->cores = calloc(ctx->stream_count, sizeof(at9_core_t));
    ctx->channels = calloc(ctx->channel_count, sizeof(at9_channel_t));
    if (!ctx->cores || !ctx->channels)
        goto fail;
    ctx->dmc_channels = info.dmc_channels;

    for (int s = 0; s < ctx->stream_count; s++) {
        at9_core_t* core = &ctx->cores[s];

        core->profile_index = info.profile_index;
        core->channel_config_index = info.channel_config_index;
        core->frames_per_superframe = 1 << info.superframe_index;
        core->superframe_size = info.frame_size << info.superframe_index;
        core->frame_samples = frame_samples_table[info.profile_index];

        ctx->superframe_size += core->superframe_size;
    }

    init_huffman_tables();

    init_state(ctx);

    return ctx;
fail:
    atrac9_free(ctx);
    return NULL;
}

void atrac9_free(atrac9_handle_t* ctx) {
    if (!ctx)
        return;
    free(ctx->cores);
    free(ctx->channels);
    free(ctx);
}

void atrac9_reset(atrac9_handle_t* ctx) {
    if (!ctx)
        return;
    // OG has no reset (decoder must be re-created?)
    init_state(ctx);
}

/* OG lib can pass offset/samples to (presumably) trim encoder delay or loop parts (handled externally here), and
 * set a 'seek flag' that only does unpack. Not too useful but probably means loops overlap with last frame's iMDCT. */
// OG: sceAt9DecDecode / sceAt9DecSpecificDecodeSplit -> at9_decode_frame / at9_decode_frameSplit
static int decode(atrac9_handle_t* ctx, const uint8_t* src, int src_size, void* dst, int output_mode, int* p_bytes_used) {

    if (!ctx || !src || src_size <= 0 || !dst)
        return AT9_ERROR_PARAMS;

    // V3: DMC frames hold one mono frame per stream back to back, each with its own core state.
    // Output is interleaved over all channels, in stream order.
    int bytes_used = 0;
    int first_channel = 0;
    int res = AT9_OK;
    for (int s = 0; s < ctx->stream_count; s++) {
        at9_core_t* core = &ctx->cores[s];
        int stream_bytes_used = 0;

        res = decode_frame(core, &ctx->block_temp, ctx->channel_temps, src + bytes_used, src_size - bytes_used, dst, output_mode,
                first_channel, ctx->channel_count, &stream_bytes_used);
        bytes_used += stream_bytes_used;
        if (res < 0)
            break;

        first_channel += core->channel_count;
    }
    if (p_bytes_used)
        *p_bytes_used = bytes_used;
    if (res < 0)
        return res;

    return ctx->cores[0].frame_samples;
}

int atrac9_decode(atrac9_handle_t* ctx, const uint8_t* src, int src_size, float* dst, int* p_bytes_used) {
    return decode(ctx, src, src_size, dst, AT9_OUTPUT_FLOAT16, p_bytes_used);
}

int atrac9_decode_float(atrac9_handle_t* ctx, const uint8_t* src, int src_size, float* dst, int* p_bytes_used) {
    return decode(ctx, src, src_size, dst, AT9_OUTPUT_FLOAT, p_bytes_used);
}

int atrac9_decode_pcm16(atrac9_handle_t* ctx, const uint8_t* src, int src_size, int16_t* dst, int* p_bytes_used) {
    return decode(ctx, src, src_size, dst, AT9_OUTPUT_PCM16, p_bytes_used);
}

static int decode_superframe(atrac9_handle_t* ctx, const uint8_t* src, int src_size, void* dst, int output_mode) {
    if (!ctx || !src || !dst || src_size <= 0)
        return AT9_ERROR_PARAMS;

    int sample_size = output_mode == AT9_OUTPUT_PCM16 ? sizeof(int16_t) : sizeof(float);
    int step_size = ctx->cores[0].frame_samples * ctx->channel_count * sample_size;

    for (int i = 0; i < ctx->cores[0].frames_per_superframe; i++) {
        int bytes_used = 0;

        int res = decode(ctx, src, src_size, dst, output_mode, &bytes_used);
        if (res < 0)
            return res;

        if (src_size < bytes_used)
            return AT9_ERROR_OVERREAD;
        src += bytes_used;
        src_size -= bytes_used;

        dst = (uint8_t*)dst + step_size;
    }

    return ctx->cores[0].frame_samples * ctx->cores[0].frames_per_superframe;
}

int atrac9_decode_superframe(atrac9_handle_t* ctx, const uint8_t* src, int src_size, float* dst) {
    return decode_superframe(ctx, src, src_size, dst, AT9_OUTPUT_FLOAT16);
}

int atrac9_decode_superframe_float(atrac9_handle_t* ctx, const uint8_t* src, int src_size, float* dst) {
    return decode_superframe(ctx, src, src_size, dst, AT9_OUTPUT_FLOAT);
}

int atrac9_decode_superframe_pcm16(atrac9_handle_t* ctx, const uint8_t* src, int src_size, int16_t* dst) {
    return decode_superframe(ctx, src, src_size, dst, AT9_OUTPUT_PCM16);
}


int atrac9_get_info(atrac9_handle_t* ctx, atrac9_info_t* dst) {
    if (!ctx || !dst)
        return AT9_ERROR_PARAMS;

    dst->channels = ctx->channel_count;
    dst->sample_rate = ctx->sample_rate;
    dst->superframe_size = ctx->superframe_size;
    dst->frames_per_superframe = ctx->cores[0].frames_per_superframe;
    dst->frame_samples = ctx->cores[0].frame_samples;
    dst->dmc_mode = ctx->dmc_channels > 0;

    return AT9_OK;
}

int atrac9_get_config_info(const uint8_t* config_data, atrac9_info_t* dst) {
    if (!config_data || !dst)
        return AT9_ERROR_PARAMS;

    at9_config_info_t info = {0};
    int res = parse_config_data(config_data, &info);
    if (res < 0)
        return AT9_ERROR_PARAMS;

    int channels = channel_config_table[info.channel_config_index].channels;
    int superframe_size = info.frame_size << info.superframe_index;
    int sample_rate = sample_rate_table[info.profile_index];
    int samples_per_superframe = 1 << info.superframe_index;
    int frame_samples = frame_samples_table[info.profile_index];

    if (info.dmc_channels) {
        channels = info.dmc_channels;
        superframe_size *= info.dmc_channels;
    }

    dst->channels = channels;
    dst->sample_rate = sample_rate;
    dst->superframe_size = superframe_size;
    dst->frames_per_superframe = samples_per_superframe;
    dst->frame_samples = frame_samples;
    dst->dmc_mode = info.dmc_channels > 0;

    return AT9_OK;
}
