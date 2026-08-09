#include <string.h>
#include "paths.h"
#include "string_utils.h"
#include "../util.h"

#ifndef DIR_SEPARATOR
    #if defined (_WIN32) || defined (WIN32)
        #define DIR_SEPARATOR '\\'
    #else
        #define DIR_SEPARATOR '/'
    #endif
#endif


#if 0 // shouldn't be needed (normalized during open_streamfile_*)
void fix_dir_separators(char* filename) {
    char c;
    int i = 0;
    while ((c = filename[i]) != '\0') {
        if ((c == '\\' && DIR_SEPARATOR == '/') || (c == '/' && DIR_SEPARATOR == '\\'))
            filename[i] = DIR_SEPARATOR;
        i++;
    }
}
#endif

void normalize_path(char* filename, size_t filename_size, char separator) {
    const int max = (int)filename_size - 1;

    if (separator == '\0')
        separator = DIR_SEPARATOR;

    char curr;
    int i = 0;
    while ((curr = filename[i]) != '\0' && i < max) {
        if ((curr == '\\' && separator == '/') || (curr == '/' && separator == '\\'))
            filename[i] = separator;
        i++;
    }
}

bool is_path_absolute(const char* path) {
    return path[0] == '/' || path[0] == '\\' || path[1] == ':';
}

void trim_path(char* path) {
    if (path[0] == '\0')
        return;

    /* remove trailing spaces */
    size_t len = strlen(path);
    for (size_t i = len - 1; i > 0; i--) {
        if (path[i] != ' ')
            break;
        path[i] = '\0';
    }
}

/* Detect used separator and last slash in current path (loaded streamfile). Used to normalize
 * paths for passed concat'd filename.
 *
 * Could just force plugins to only accept / but to simplify things handle all this stuff:
 * - foobar2000 uses paths like "file://C:\path\to\file.ext"
 * - Winamp uses paths like "C:\path\to\file.ext", may include a (protocol)://... prefix
 * - Windows CMD can use both \ and /
 * - and so on
 */
void get_path_info(char* fullname, char** p_path_end, char* p_separator) {
    char* path_end;

    char* fwd_slash = strrchr(fullname, '/');
    char* bck_slash = strrchr(fullname, '\\');

    char separator;
    if (bck_slash && fwd_slash) {
        if (bck_slash > fwd_slash) {
            path_end = bck_slash;
            separator = '\\';
        }
        else {
            path_end = fwd_slash;
            separator = '/';
        }
    }
    else if (bck_slash) {
        path_end = bck_slash;
        separator = '\\';
    }
    else if (fwd_slash) {
        path_end = fwd_slash;
        separator = '/';
    }
    else {
        path_end = NULL;
        separator = DIR_SEPARATOR;
    }

    *p_path_end = path_end;
    *p_separator = separator;
}

bool is_restricted_path(const char* filename) {
    if (!filename || filename[0] == '\0')
        return false;

    // absolute Unix / Windows (net) path
    if (filename[0] == '/' || filename[0] == '\\')
        return true;

    for (size_t i = 0; filename[i] != '\0'; i++) {
        char curr = filename[i];
        char next = filename[i + 1];

        // :/ or :\ (Windows drive / url)
        if (curr == ':' && (next == '/' || next == '\\')) {
            return true;
        }

        // relative path
        if (curr == '.' && next == '.') {
            char next2 = filename[i + 2];
            if (next2 == '/' || next2 == '\\') {
                return true;
            }
        }
    }

    return false;
}


//TODO remove
static void make_uppercase(char* str) {
    if (str == NULL)
        return;

    while (str[0] != '\0') {
        char c = str[0];
        if (c >= 'a' && c <= 'z') {
            str[0] = c - 0x20;
        }
        str++;
    }
}

/* change pathname's extension to another, or add it if extensionless */
void swap_extension(char* dst, size_t dst_size, const char* new_ext) {
    if (dst == NULL || dst_size == 0 || new_ext == NULL)
        return;

    char* extension = (char*)filename_extension(dst);
    if (!extension)
        return;

    // probably unnecessary but might as well
    ptrdiff_t dst_offset = extension - dst;
    if (dst_offset < 0 || (size_t)dst_offset > dst_size)
        return;
    size_t extension_size = dst_size - (size_t)dst_offset;

    bool ext_uppercase = new_ext[0] != '\0' && str_is_uppercase(extension);

    if (extension[0] == '\0') {
        // dst has no extension, add new one
        if (new_ext[0] != '\0') {
            strcat_v(dst, dst_size, ".");
            strcat_v(dst, dst_size, new_ext);
        }
    }
    else {
        // dst has extension, replace with new one
        if (new_ext[0] != '\0') {
            strcpy_v(extension, extension_size, new_ext);
        }
        else {
            // no extension, just remove original extension
            if (extension > dst && extension[-1] == '.')
                extension[-1] = '\0';
            //extension--;
            //extension[0] = '\0';
        }
    }

    // try to match original case so Linux may work
    if (ext_uppercase) {
        make_uppercase(extension);
    }
}
