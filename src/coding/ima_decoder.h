#include "../vgmstream.h"

#ifndef _IMA_DECODER_H
#define _IMA_DECODER_H

void decode_nds_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

#endif
