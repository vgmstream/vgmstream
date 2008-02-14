#ifndef _G721_decoder_H
#define	_G721_decoder_H

#include "../vgmstream.h"
#include "../streamtypes.h"

void decode_g721(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

#endif
