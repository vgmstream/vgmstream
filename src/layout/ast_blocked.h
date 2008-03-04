/*
 * ast_blocked.h - AST blocking
 */
#include "../streamtypes.h"
#include "../vgmstream.h"

#ifndef _AST_BLOCKED_H
#define _AST_BLOCKED_H

void render_vgmstream_ast_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void ast_block_update(off_t block_ofset, VGMSTREAM * vgmstream);

#endif
