#ifndef _PATHS_H
#define _PATHS_H
#include <stddef.h>
#include <stdbool.h>

#if 0
/* hack to allow relative paths in various OSs */
void fix_dir_separators(char* filename);
#endif

void normalize_path(char* filename, size_t filename_size, char separator);

bool is_path_absolute(const char* path);

void trim_path(char* filename);

void get_path_info(char* fullname, char** p_path_end, char* p_separator);

bool is_restricted_path(const char* filename);

void swap_extension(char* dst, size_t dst_size, const char* new_ext);

//const char* filename_extension(const char* pathname);

#endif
