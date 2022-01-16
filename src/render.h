#ifndef _RENDER_H
#define _RENDER_H

#include "vgmstream.h"

void free_layout(VGMSTREAM* vgmstream);
void reset_layout(VGMSTREAM* vgmstream);
int render_layout(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream);


#endif
