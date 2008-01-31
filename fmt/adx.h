/*
 * adx.h - ADX reading and decoding
 */

#include "../vgmstream.h"

#ifndef _ADX_H
#define _ADX_H

VGMSTREAM * init_vgmstream_adx(const char * const filename);
void decode_adx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

#endif
