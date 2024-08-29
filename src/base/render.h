#ifndef _RENDER_H
#define _RENDER_H

#include "../vgmstream.h"
#include "sbuf.h"

void render_free(VGMSTREAM* vgmstream);
void render_reset(VGMSTREAM* vgmstream);
int render_layout(sbuf_t* sbuf, VGMSTREAM* vgmstream);
int render_main(sbuf_t* sbuf, VGMSTREAM* vgmstream);


#endif
