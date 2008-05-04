
#include "../vgmstream.h"

#ifndef _PSX_DECODER_H
#define _PSX_DECODER_H

void decode_psx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

#endif
