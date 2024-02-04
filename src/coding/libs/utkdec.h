#ifndef _UTKDEK_H_
#define _UTKDEK_H_
#include <stdint.h>

/* Decodes Electronic Arts' MicroTalk (a multipulse CELP/RELP speech codec) using utkencode lib,
 * slightly modified for vgmstream based on decompilation of EA and CBX code.
 * Original by Andrew D'Addesio: https://github.com/daddesio/utkencode (UNLICENSE/public domain)
 * Info: http://wiki.niotso.org/UTK
 *
 * EA classifies MT as MT10:1 (smaller frames) and MT5:1 (bigger frames), but both are the same
 * with different encoding parameters. Later revisions may have PCM blocks (rare). This codec was
 * also reused by Traveller Tales in CBX (same devs?) with minor modifications.
 * Internally it's sometimes called "UTalk" too.
 *
 * TODO:
 * - lazy/avoid peeking/overreading when no bits left (OG code does it though, shouldn't matter)
 * - same with read_callback (doesn't affect anything but cleaner)
 */

typedef enum {
    UTK_EA,     // standard EA-MT (MT10 or MT5)
    UTK_EA_PCM, // EA-MT with PCM blocks
    UTK_CBX,    // Traveller's Tales Chatterbox
} utk_type_t;

/* opaque struct */
typedef struct utk_context_t utk_context_t;

/* inits UTK */
utk_context_t* utk_init(utk_type_t type);

/* frees UTK */
void utk_free(utk_context_t*);

/* reset/flush */
void utk_reset(utk_context_t* ctx);

/* loads current data (can also be used to reset buffered data if set to 0) */
void utk_set_buffer(utk_context_t* ctx, const uint8_t* buf, size_t buf_size);

/* prepares for external streaming (buf is where reads store data, arg is any external params for the callback) */
void utk_set_callback(utk_context_t* ctx, uint8_t* buf, size_t buf_size, void* arg, size_t (*read_callback)(void*, int , void*));

/* main decode; returns decoded samples on ok (always >0), < 0 on error */
int utk_decode_frame(utk_context_t* ctx);

/* get sample buf (shouldn't change between calls); sample type is PCM float (+-32768 but not clamped) */
float* utk_get_samples(utk_context_t* ctx);

#endif
