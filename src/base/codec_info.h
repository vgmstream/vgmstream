#ifndef _CODEC_INFO_H
#define _CODEC_INFO_H

#include <stdint.h>
#include <stdbool.h>
#include "../vgmstream.h"
#include "../base/sbuf.h"

/* Class-like definition for codecs.
 */
typedef struct {
    //int (*init)();
    sfmt_t sample_type;                         // fixed for most cases; if not set will be assumed to be PCM16
    sfmt_t (*get_sample_type)(VGMSTREAM* v);    //variable for codecs with variations depending on data

    bool (*decode_frame)(VGMSTREAM* v);
    void (*free)(void* codec_data);
    void (*reset)(void* codec_data);
    void (*seek)(VGMSTREAM* v, int32_t num_sample);

    bool (*decode_buf)(VGMSTREAM* v, sbuf_t* sdst);         // alternate decoding for codecs that don't provide their own buffer

    bool (*seekable)(VGMSTREAM* v); // if codec may seek to arbitrary samples using ->seek (defaults to slow seek otherwise)
                                    // ->seek is typically only been tested with loops, returning true here meant it can be used

    // info for vgmstream
    //uint32_t flags; 
    // alloc size of effect's private data (don't set to manage manually in init/free)
    //int priv_size;

    //int sample_type;
    //int get_sample_type();
} codec_info_t;


const codec_info_t* codec_get_info(VGMSTREAM* v);

#endif
