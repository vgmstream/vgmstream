#ifndef _SEEK_H_
#define _SEEK_H_

#include "../vgmstream.h"

/* Seek to sample position (next render starts from that point). 
 * Use only after config is set (vgmstream_apply_config) */
void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample);

#endif
