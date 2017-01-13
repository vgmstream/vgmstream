/*
 * header.h - utilities for common/repetitive actions in stream headers (more complex than those in util.h)
 */

#ifndef _HEADER_H_
#define _HEADER_H_

#include "util.h"
#include "streamfile.h"
#include "vgmstream.h"


int header_check_extensions(STREAMFILE *streamFile, const char * cmpexts);

int header_open_stream(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t start_offset);

void header_dsp_read_coefs_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);

#endif /* _HEADER_H_ */
