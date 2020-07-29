#ifndef _DECODE_H
#define _DECODE_H

#include "vgmstream.h"

void free_codec(VGMSTREAM* vgmstream);
void seek_codec(VGMSTREAM* vgmstream);
void reset_codec(VGMSTREAM* vgmstream);

/* Decode samples into the buffer. Assume that we have written samples_written into the
 * buffer already, and we have samples_to_do consecutive samples ahead of us. */
void decode_vgmstream(VGMSTREAM* vgmstream, int samples_written, int samples_to_do, sample_t* buffer);

/* Detect loop start and save values, or detect loop end and restore (loop back). Returns 1 if loop was done. */
int vgmstream_do_loop(VGMSTREAM* vgmstream);

/* Calculate number of consecutive samples to do (taking into account stopping for loop start and end) */
int get_vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM* vgmstream);


/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int get_vgmstream_samples_per_frame(VGMSTREAM* vgmstream);

/* Get the number of bytes of a single frame (smallest self-contained byte group, 1/N channels) */
int get_vgmstream_frame_size(VGMSTREAM* vgmstream);

/* In NDS IMA the frame size is the block size, but last one is shorter */
int get_vgmstream_samples_per_shortframe(VGMSTREAM* vgmstream);
int get_vgmstream_shortframe_size(VGMSTREAM* vgmstream);


#endif
