/*
 * interleave.h - interleaved layouts
 */
#include "../streamtypes.h"
#include "../vgmstream.h"

#ifndef _NOLAYOUT_H
#define _NOLAYOUT_H

void render_vgmstream_nolayout(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

#endif
