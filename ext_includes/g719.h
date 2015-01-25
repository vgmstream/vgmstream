/*
    Interface to reference G.719 decoder
*/

#ifndef G719_H
#define G719_H

/* forward definition for the opaque handle object */
typedef struct g719_handle_s g719_handle;

/* return a handle for decoding on successful init, NULL on failure */
g719_handle * g719_init(int sample_rate);

/* decode a frame, at code_words, into 16-bit PCM in sample_buffer */
void g719_decode_frame(g719_handle *handle, void *code_words, void *sample_buffer);

/* reset the decoder to its initial state */
void g719_reset(g719_handle *handle);

/* free resources */
void g719_free(g719_handle *handle);

#endif
