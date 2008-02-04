#include "../vgmstream.h"

#ifndef _GCDSP_H
#define _GCDSP_H

void decode_gcdsp(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

#endif
