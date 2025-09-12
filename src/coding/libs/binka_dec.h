#ifndef _BINKA_DEC_
#define _BINKA_DEC_

/* Decodes RAD Game Tools's Bink Audio, a fairly simple VBR transform-based codec. 
 * Reverse engineered from various libs/exes (mainly binkawin.asi/radutil.dll).
 *
 * References:
 * - https://wiki.multimedia.cx/index.php/Bink_Audio
 * - https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/binkaudio.c
 * - https://github.com/scummvm/scummvm/blob/master/video/bink_decoder.cpp
 */

//#define MAX_FRAME_SAMPLES_OUTPUT  1920

typedef struct binka_handle_t binka_handle_t;

typedef enum { /*BINKA_VIDEO_10, BINKA_VIDEO_11,*/ BINKA_BFC, BINKA_UEBA } binka_mode_t;

/* Inits decoder, with slightly different modes.
 */
binka_handle_t* binka_init(int sample_rate, int channels, binka_mode_t mode);

void binka_free(binka_handle_t* handle);

/* Setup decoder. After resetting next frame is not windowed with prev frame
 * (after seeking probably shouldn't reset, unless seeking to 0).
 */
void binka_reset(binka_handle_t* handle);

/* Decodes one Bink Audio packet.
 * Returns samples done, 0 if more packets are needed, negative on error.
 * For >2ch it needs (channels + 1) / 2 packets (each of 1/2ch) before it can output all samples,
 * so it returns 0 until all packets have been fed.
 *
 * src should have raw data for 1 packet (without sync or size seen in BCF1/UEBA).
 * dst should have frame_samples * channels.
 */
int binka_decode(binka_handle_t* handle, unsigned char* src, int src_size, float* dst);


// Get current frame size for one single frame.
// (mainly useful for the seek table).
int binka_get_frame_samples(binka_handle_t* ctx);

#endif
