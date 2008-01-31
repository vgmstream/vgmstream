/*
 * interleave.h - interleaved layouts
 */
#include "../streamtypes.h"
#include "../vgmstream.h"

#ifndef _INTERLEAVE_H
#define _INTERLEAVE_H

void render_vgmstream_interleave(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

#endif
