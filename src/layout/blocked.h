/*
 * blocked.h - blocking
 */
#include "../streamtypes.h"
#include "../vgmstream.h"

#ifndef _BLOCKED_H
#define _BLOCKED_H

void render_vgmstream_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

#endif
