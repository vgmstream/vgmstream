#ifndef _PATHS_H
#define _PATHS_H

#ifndef DIR_SEPARATOR
    #if defined (_WIN32) || defined (WIN32)
        #define DIR_SEPARATOR '\\'
    #else
        #define DIR_SEPARATOR '/'
    #endif
#endif

/* hack to allow relative paths in various OSs */
void fix_dir_separators(char* filename);

//const char* filename_extension(const char* pathname);

#endif
