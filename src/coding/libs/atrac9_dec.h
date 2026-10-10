#ifndef _ATRAC9_DEC_
#define _ATRAC9_DEC_

#include <stdint.h>

/* Decodes Sony's ATRAC9, a transform-based (MDCT).
 */

//TO-DO: remove?
#define ATRAC9_CONFIG_SIZE          4
#define ATRAC9_MAX_SUPERFRAME_SIZE  0x800 // 0x200*4
#define ATRAC9_MAX_FRAME_SAMPLES    256 // single frame only

typedef struct atrac9_handle_t atrac9_handle_t;

/* Inits decoder from features packed in config data, that must be 4 bytes.
 */
atrac9_handle_t* atrac9_init(const uint8_t* config_data);

void atrac9_free(atrac9_handle_t* handle);

void atrac9_reset(atrac9_handle_t* handle);

/* Decodes one frame into interleaved pcm. Encoder delay samples are handled externally.
 * Samples are in unclamped PCM16 floats (+-32767.0 like the internal bufs of the OG lib,
 * albeit interleaved). Use decode_float() for +-1.0 float or decode_pcm16() for clamped PCM.
 *
 * Caller must read superframes and decode + advance frame by frame manually, based on returned bytes_used.
 * - src: current frame (within a superframe).
 * - dst: at least frame_samples * channels
 * - p_bytes_used: returns bytes consumed from src (frame size, including superframe padding on last frame)
 * Returns samples done (frame_samples) or negative on error.
 */
int atrac9_decode(atrac9_handle_t* handle, const uint8_t* src, int src_size, float* dst, int* p_bytes_used);

int atrac9_decode_float(atrac9_handle_t* handle, const uint8_t* src, int src_size, float* dst, int* p_bytes_used);

int atrac9_decode_pcm16(atrac9_handle_t* handle, const uint8_t* src, int src_size, int16_t* dst, int* p_bytes_used);

/* Helper that decodes a whole superframe.
 * - src: current superframe
 * - dst: at least frame_samples * frames_per_superframe * channels
 */
int atrac9_decode_superframe(atrac9_handle_t* handle, const uint8_t* src, int src_size, float* dst);

int atrac9_decode_superframe_float(atrac9_handle_t* handle, const uint8_t* src, int src_size, float* dst);

int atrac9_decode_superframe_pcm16(atrac9_handle_t* handle, const uint8_t* src, int src_size, int16_t* dst);

typedef struct {
    int channels;
    int sample_rate;
    int superframe_size;
    int frames_per_superframe;
    int frame_samples;
    int dmc_mode;
} atrac9_info_t;

int atrac9_get_info(atrac9_handle_t* ctx, atrac9_info_t* dst);
int atrac9_get_config_info(const uint8_t* config_data, atrac9_info_t* dst);

#endif
