#ifndef _RENDER_H
#define _RENDER_H

#include "../vgmstream.h"

void render_free(VGMSTREAM* vgmstream);
void render_reset(VGMSTREAM* vgmstream);
int render_layout(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream);


#endif
