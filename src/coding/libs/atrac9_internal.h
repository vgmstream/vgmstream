#ifndef _ATRAC9_INTERNAL_H_
#define _ATRAC9_INTERNAL_H_

#include <stdint.h>
#include "../../util/bitstream_msb.h"

#define AT9_MAX_CHANNELS_DMC    64
#define AT9_MAX_CHANNELS_STD    8

#define AT9_MAX_FRAME_SIZE      0x200
#define AT9_MAX_BLOCKS          5
#define AT9_MAX_BLOCK_CHANNELS  2
#define AT9_MAX_QUS             30
#define AT9_MAX_COEFS           256

enum {
    AT9_BLOCK_MONO = 0,
    AT9_BLOCK_STEREO = 1,
    AT9_BLOCK_LFE = 2,
    AT9_BLOCK_VIBRATION = 3,
};

/* OG error codes */
#define AT9_OK                  0
#define AT9_ERROR_PARAMS        -1
#define AT9_ERROR_UNPACK        -2
#define AT9_ERROR_OVERREAD      -3
#define AT9_ERROR_PADDING       -4
#define AT9_ERROR_TRANSFORM     -5


//TODO: simplify channel_temps vs channel_temp
//TODO: simplify  at9_core_t + at9_channel_temp_t + at9_block_temp_t

/* band extension state */
// OG: BEX
typedef struct {
    int16_t method;                         // (OG: method)
    int16_t value_count;                    // (OG: nvqc)
    int16_t values[4];                      // (OG: a_vqc)
    uint16_t rng[4];                        // (OG: a_rfac)
} at9_bex_t;

/* persistent channel state */
// OG: AC (Audio Channel) (inside SceAt9DecChWork)
typedef struct {
    float overlap[AT9_MAX_COEFS / 2];       // (OG: a_sigprocContext)
    uint8_t sf_indexes_prev[AT9_MAX_QUS];   // (OG: a_idsf_prev, short unlike a_idsf)
    at9_bex_t bex;
} at9_channel_t;

/* persistent block state */
// OG: AB (audio block)
typedef struct {
    int type;                               // not in OG (derived from passed channel_config)
    int channel_count;                      // not in OG
    int qu_count;                           // (OG: nqus)
    int qu_count_prev;                      // (OG: nqus_prev)
    int ext_enabled;                        // (OG: ext_flag)
    int bex_enabled;                        // (OG: exswitch)
    int bex_qu_count;                       // (OG: nexqus)
    int stereo_qu_count;                    // (OG: isqus)
    at9_channel_t* channels[AT9_MAX_BLOCK_CHANNELS]; // OG: ap_ac
} at9_block_t;

/* decoder state for a single frame */
// OG: SceAt9DecCoreWorkInt
typedef struct {
    // (OG keeps the bitreader here (puStream + loc), but it is passed as a parameter to unpack functions instead)
    int profile_index;                      // sample rate + frame size mode (OG: profile)
    int channel_config_index;               // (OG: channel_config_index)
    int superframe_size;                    // (OG: supframe_size)
    int frames_per_superframe;              // (OG: nFramesInSuperframe)
    int current_frame;                      // (OG: frame_cnt)
    int superframe_bytes_used;              // (OG: nbytes_used)

    int block_count;
    int channel_count;
    int frame_samples;
    at9_block_t blocks[AT9_MAX_BLOCKS];     // (OG: ab)
} at9_core_t;


/* temp block data, that can be reused for all blocks during decode */
// OG: SceAbTmp (inside SceAt9DecTmpWork)
typedef struct {
    int gradient[AT9_MAX_QUS];              // (OG: a_grad)
    int wl_adjust[AT9_MAX_QUS];             // (OG: a_adwl)
    int frame_id;                           // 0 = first frame in superframe (OG: frame_id)
    int gradient_mode;                      // (OG: grad_mode)
    int gradient_qu_lo;                     // (OG: grad_qu_l)
    int gradient_qu_hi;                     // (OG: grad_qu_h)
    int gradient_os_lo;                     // (OG: grad_os_l)
    int gradient_os_hi;                     // (OG: grad_os_h)
    int adjust_qu_count;                    // (OG: nadjqus)
    uint8_t stereo_signs[AT9_MAX_QUS];      // (OG: is_phase)
    int main_channel;                       // 0 = left, 1 = right (OG: main_ch)
} at9_block_temp_t;

/* temp channel data (reused per block channel during decode) */
// OG: SceAcTmp (inside SceAt9DecTmpWork)
typedef struct {
    float samples[AT9_MAX_COEFS];           // (OG: a_sigproc)
    float spectrum[AT9_MAX_COEFS];          // (OG: a_tmp_mdspec)
    uint8_t sf_indexes[AT9_MAX_QUS];        // (OG: a_idsf, int)
    uint8_t codebook_sets[AT9_MAX_QUS];     // (OG: a_ptflag)
    int wl_indexes[AT9_MAX_QUS];            // (OG: a_idwl)
    int cl_indexes[AT9_MAX_QUS];            // (OG: a_cidwl) (c=component?)
} at9_channel_temp_t;


/* High sample rate modes (8..15) aren't in OG, so this is adapted from Sony's tools. */
static inline int is_high_sample_rate(const at9_core_t* core) {
    return core->profile_index > 7;
}

void init_huffman_tables(void);
int unpack_block(at9_core_t* core, bitstream_t* is, at9_block_temp_t* block_temp, at9_block_t* block, at9_channel_temp_t* channel_temps);
int unpack_superframe_padding(bitstream_t* is, int bytes_left);
int transform_block(at9_core_t* core, at9_block_t* block, at9_channel_temp_t* channel_temps);

#endif
