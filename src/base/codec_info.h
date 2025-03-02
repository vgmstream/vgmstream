#ifndef _CODEC_INFO_H
#define _CODEC_INFO_H

#include <stdint.h>
#include <stdbool.h>
#include "../vgmstream.h"

/* Class-like definition for codecs.
 */
typedef struct {
    char const* name;
    
    //int (*init)();
    bool (*decode_frame)(VGMSTREAM* v);
    void (*free)(void* codec_data);
    void (*reset)(void* codec_data);
    void (*seek)(VGMSTREAM* v, int32_t num_sample);

    // info for vgmstream
    //uint32_t flags; 
    // alloc size of effect's private data (don't set to manage manually in init/free)
    //int priv_size;

    //int sample_type;
    //int get_sample_type();
} codec_info_t;


const codec_info_t* codec_get_info(VGMSTREAM* v);

#endif
