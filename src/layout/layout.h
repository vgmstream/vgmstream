#ifndef _LAYOUT_H
#define _LAYOUT_H

#include "../streamtypes.h"
#include "../vgmstream.h"

void ast_block_update(off_t block_ofset, VGMSTREAM * vgmstream);

void render_vgmstream_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void halpst_block_update(off_t block_ofset, VGMSTREAM * vgmstream);

void render_vgmstream_interleave(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void render_vgmstream_nolayout(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

#endif
