/*
 * formats.h - utils to parse supported formats
 */
#ifndef _FORMATS_H_
#define _FORMATS_H_

#include "vgmstream.h"

/* rough number of chars counting all extensions (actually <1500 and extra space) */
#define VGM_EXTENSION_LIST_CHAR_SIZE   2000

const char ** vgmstream_get_formats();
int vgmstream_get_formats_length();

const char * get_vgmstream_coding_description(coding_t coding_type);
const char * get_vgmstream_layout_description(layout_t layout_type);
const char * get_vgmstream_meta_description(meta_t meta_type);

#endif /* _FORMATS_H_ */
