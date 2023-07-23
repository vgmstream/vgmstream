#ifndef _LAYOUTS_UTIL_H
#define _LAYOUTS_UTIL_H

#include "../vgmstream.h"

/* add a new layer from codec data (setups layout if needed) 
 * codec is passed in the vs for easier free/etc control */
bool layered_add_codec(VGMSTREAM* vs, int layers, int layer_channels);

/* call when done adding layers */
bool layered_add_done(VGMSTREAM* vs);
#endif
