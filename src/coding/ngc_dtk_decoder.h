#include "../vgmstream.h"

#ifndef _NGC_DTK_DECODER_H
#define _NGC_DTK_DECODER_H

void decode_ngc_dtk(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

#endif
