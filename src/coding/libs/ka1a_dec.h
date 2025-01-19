#ifndef _KA1A_DEC_
#define _KA1A_DEC_

/* Decodes Koei Tecmo's KA1A, a fairly simple transform-based (FFT) mono codec. */


//#define KA1A_FRAME_SIZE_MAX 0x200
#define KA1A_FRAME_SAMPLES 512


typedef struct ka1a_handle_t ka1a_handle_t;

/* Inits decoder.
 * - bitrate_mode: value from header (-5..5)
 * - channels: Nch-interleaved tracks
 * - tracks: number of parts of N-ch
 *
 * Channel/tracks define final interleaved output per ka1a_decode:
 *    [track0 ch0 ch1 ch0 ch1... x512][track1 ch0 ch1 ch0 ch1... x512]...
 * Codec is mono though, so this can be safely reinterpreted, ex. channels = tracks * channels, tracks = 1:
 *    [track0 ch0 ch1 ch3 ch4 ch5 ch6... x512]
 * or even make N single decoders per track/channel and pass single frames.
 */
ka1a_handle_t* ka1a_init(int bitrate_mode, int channels, int tracks);

void ka1a_free(ka1a_handle_t* handle);

void ka1a_reset(ka1a_handle_t* handle);

/* Decodes one block of data.
 * Returns samples done, 0 on setup or negative or error.
 * After init/reset next decode won't input samples (similar to encoder delay).
 *
 * src should have frame_size * channels * tracks.
 * dst should have KA1A_FRAME_SAMPLES * channels * tracks (see init for interleave info).
 */
int ka1a_decode(ka1a_handle_t* handle, unsigned char* src, float* dst);

// Get current frame size for one single frame.
int ka1a_get_frame_size(ka1a_handle_t* handle);

#endif
