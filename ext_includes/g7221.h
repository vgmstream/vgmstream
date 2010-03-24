/*
    Interface to reference G.722.1 decoder
*/

#ifndef G7221_H
#define G7221_H

/* forward definition for the opaque handle object */
typedef struct g7221_handle_s g7221_handle;

/* return a handle for decoding on successful init, NULL on failure */
g7221_handle * g7221_init(int bytes_per_frame, int bandwidth);

/* decode a frame, at code_words, into 16-bit PCM in sample_buffer */
void g7221_decode_frame(g7221_handle *handle, int16_t *code_words, int16_t *sample_buffer);

/* reset the decoder to its initial state */
void g7221_reset(g7221_handle *handle);

/* free resources */
void g7221_free(g7221_handle *handle);

#endif
