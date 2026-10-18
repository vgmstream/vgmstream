#ifndef _G719_DEC_H
#define _G719_DEC_H

/* Decodes Namco's (Polycom's) IS22 a.k.a. G719, a transform-based codec (MDCT).
 */

#define G719_MAX_FRAME_SIZE 0x140
#define G719_MAX_FRAME_SAMPLES 960

typedef struct g719_handle_t g719_handle_t;

/* Inits decoder from sample rate
 */
g719_handle_t* g719_init(int sample_rate);

void g719_free(g719_handle_t* handle);

void g719_reset(g719_handle_t* handle);

/* Decodes one frame into interleaved pcm.
 */
void g719_decode_frame(g719_handle_t* handle, unsigned char* code_words, short* sample_buffer);

#endif
