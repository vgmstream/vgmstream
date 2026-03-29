#ifndef _PLAY_STATE_H_
#define _PLAY_STATE_H_

#include "../vgmstream.h"

int32_t vgmstream_get_samples(VGMSTREAM* vgmstream);
int vgmstream_get_play_forever(VGMSTREAM* vgmstream);
void vgmstream_set_play_forever(VGMSTREAM* vgmstream, int enabled);

#endif
