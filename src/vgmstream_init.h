#ifndef _DETECTION_H_
#define _DETECTION_H_

#include "meta/meta.h"
#include "vgmstream.h"

bool prepare_vgmstream(VGMSTREAM* vgmstream, STREAMFILE* sf);
VGMSTREAM* detect_vgmstream_format(STREAMFILE* sf);
init_vgmstream_t get_vgmstream_format_init(int format_id);

#endif
