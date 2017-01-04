/*
 * formats.h - utils to parse supported formats
 */
#ifndef _FORMATS_H_
#define _FORMATS_H_


/* rough number of chars counting all extensions (actually <1500 and extra space) */
#define VGM_EXTENSION_LIST_CHAR_SIZE   2000

const char ** vgmstream_get_formats();
int vgmstream_get_formats_length();


#endif /* _FORMATS_H_ */
