#include "../vgmstream.h"

#ifndef _NGC_AFC_H
#define _NGC_AFC_H

void decode_ngc_afc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

#endif
