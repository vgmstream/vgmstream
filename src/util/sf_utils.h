#ifndef _SF_UTILS_H
#define _SF_UTILS_H
#include "../streamfile.h"



/* Opens a STREAMFILE from a base pathname + new extension
 * Can be used to get companion headers. */
STREAMFILE* open_streamfile_by_ext(STREAMFILE* sf, const char* ext);

/* Opens a STREAMFILE from a base path + new filename.
 * Can be used to get companion files. Relative paths like
 * './filename', '../filename', 'dir/filename' also work. */
STREAMFILE* open_streamfile_by_filename(STREAMFILE* sf, const char* filename);

/* various STREAMFILE helpers functions */

/* Checks if the stream filename is one of the extensions (comma-separated, ex. "adx" or "adx,aix").
 * Empty is ok to accept files without extension ("", "adx,,aix"). Returns 0 on failure */
int check_extensions(STREAMFILE* sf, const char* cmp_exts);

/* filename helpers */
void get_streamfile_name(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_filename(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_basename(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_path(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_ext(STREAMFILE* sf, char* buf, size_t size);

#endif
