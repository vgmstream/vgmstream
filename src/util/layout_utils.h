#ifndef _LAYOUTS_UTIL_H
#define _LAYOUTS_UTIL_H

#include "../vgmstream.h"
#include "../layout/layout.h"

typedef VGMSTREAM* (*init_vgmstream_t)(STREAMFILE*);

/* add a new layer from subfile (setups layout if needed) */
bool layered_add_subfile(VGMSTREAM* vs, int layers, int layer_channels, STREAMFILE* sf, uint32_t offset, uint32_t size, const char* ext, init_vgmstream_t init_vgmstream);


/* add a new layer from base vgmstream (setups layout if needed)  */
bool layered_add_sf(VGMSTREAM* vs, int layers, int layer_channels, STREAMFILE* sf);

/* add a new layer from codec data (setups layout if needed) 
 * codec is passed in the vs for easier free/etc control */
bool layered_add_codec(VGMSTREAM* vs, int layers, int layer_channels);

/* call when done adding layers */
bool layered_add_done(VGMSTREAM* vs);

VGMSTREAM* allocate_layered_vgmstream(layered_layout_data* data);
VGMSTREAM* allocate_segmented_vgmstream(segmented_layout_data* data, int loop_flag, int loop_start_segment, int loop_end_segment);


typedef struct {
    off_t offset;
} blocked_counter_t;

void blocked_count_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, blocked_counter_t* cfg);
#endif
