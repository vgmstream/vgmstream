#ifndef _DECODE_H
#define _DECODE_H

#include "../vgmstream.h"

void* decode_init();
void decode_free(VGMSTREAM* vgmstream);
void decode_seek(VGMSTREAM* vgmstream, int32_t sample);
void decode_loop(VGMSTREAM* vgmstream);
void decode_reset(VGMSTREAM* vgmstream);

/* Decode samples into the buffer. Assume that we have written samples_filled into the
 * buffer already, and we have samples_to_do consecutive samples ahead of us. */
void decode_vgmstream(sbuf_t* sdst, VGMSTREAM* vgmstream, int samples_to_do);

/* Detect loop start and save values, or detect loop end and restore (loop back). Returns true if loop was done. */
bool decode_do_loop(VGMSTREAM* vgmstream);

/* Calculate number of consecutive samples to do (taking into account stopping for loop start and end) */
int decode_get_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM* vgmstream);


/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int decode_get_samples_per_frame(VGMSTREAM* vgmstream);

/* Get the number of bytes of a single frame (smallest self-contained byte group, 1/N channels) */
int decode_get_frame_size(VGMSTREAM* vgmstream);

/* In NDS IMA the frame size is the block size, but last one is shorter */
int decode_get_samples_per_shortframe(VGMSTREAM* vgmstream);
int decode_get_shortframe_size(VGMSTREAM* vgmstream);


bool decode_uses_internal_offset_updates(VGMSTREAM* vgmstream);

#endif
