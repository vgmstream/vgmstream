#ifndef _COMPANION_FILES_H
#define _COMPANION_FILES_H

#include "../streamfile.h"

/* Opens a file containing decryption keys and copies to buffer.
 * Tries "(name.ext)key" (per song), "(.ext)key" (per folder) keynames.
 * returns size of key if found and copied */
size_t read_key_file(uint8_t* buf, size_t buf_size, STREAMFILE* sf);

/* Opens .txtm file containing file:companion file(-s) mappings and tries to see if there's a match
 * then loads the associated companion file if one is found */
STREAMFILE* read_filemap_file(STREAMFILE *sf, int file_num);
STREAMFILE* read_filemap_file_pos(STREAMFILE *sf, int file_num, int* p_pos);

#endif
