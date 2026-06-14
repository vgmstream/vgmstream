#ifndef _RENDER_H
#define _RENDER_H

#include "../vgmstream.h"
#include "sbuf.h"
#include "rc.h"

void render_free(VGMSTREAM* vgmstream);
void render_reset(VGMSTREAM* vgmstream);
rc_t render_layout(sbuf_t* sbuf, VGMSTREAM* vgmstream);
rc_t render_main(sbuf_t* sbuf, VGMSTREAM* vgmstream);


#endif
