#ifndef _INFO_H
#define _INFO_H

#include "../vgmstream.h"

/* Write a description of the stream into array pointed by desc, which must be length bytes long.
 * Will always be null-terminated if length > 0 */
void describe_vgmstream(VGMSTREAM* vgmstream, char* description, int length);

/* Return the average bitrate in bps of all unique files contained within this stream. */
int get_vgmstream_average_bitrate(VGMSTREAM* vgmstream);

/* Get description info */
void get_vgmstream_coding_description(VGMSTREAM* vgmstream, char* out, size_t out_size);
void get_vgmstream_layout_description(VGMSTREAM* vgmstream, char* out, size_t out_size);
void get_vgmstream_meta_description(VGMSTREAM* vgmstream, char* out, size_t out_size);

#endif
