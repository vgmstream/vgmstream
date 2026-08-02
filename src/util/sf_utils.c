#include "sf_utils.h"
#include "../vgmstream.h"
#include "reader_sf.h"
#include "paths.h"
#include "log.h"
#include "string_utils.h"


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
static void swap_extension(char* dst, size_t dst_size, const char* new_ext) {
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

STREAMFILE* open_streamfile_by_ext(STREAMFILE* sf, const char* ext) {
    char filename[PATH_LIMIT];

    get_streamfile_name(sf, filename, sizeof(filename));

    swap_extension(filename, sizeof(filename), ext);

    return open_streamfile(sf, filename);
}

static STREAMFILE* open_streamfile_internal(STREAMFILE* sf, const char* filename, bool allow_relpaths) {
    char fullname[PATH_LIMIT];
    char partname[PATH_LIMIT];
    char *path, *name, *otherpath;

    if (!sf || !filename || !filename[0])
        return NULL;

    // Some formats open companion files in current dir or subfolders. Relative paths are restricted by default
    // for better control access. Mainly for doc purposes (most formats wouldn't need that), since
    // the security risk of a format opening an arbitrary file is low given regular .m3u allow arbitrary paths
    // anyway, and without RCE it can't leave your system (and web player can't read the filesystem).
    if (!allow_relpaths) {
        // disallow relative or absolute paths (crafty users could mimic plugin's internal URIs though)
        if (strstr(filename, "..") || strstr(filename, ":\\") || filename[0] == '/') {
            VGM_LOG("SF: ignored relative path %s\n", filename);
            return NULL;
        }
    }

    get_streamfile_name(sf, fullname, sizeof(fullname));

    //todo normalize separators in a better way, safeops, improve copying

    /* check for non-normalized paths first (ex. txth) */
    path = strrchr(fullname, '/');
    otherpath = strrchr(fullname, '\\');
    if (otherpath > path) { //todo cast to ptr?
        /* foobar makes paths like "(fake protocol)://(windows path with \)".
         * Hack to work around both separators, though probably foo_streamfile
         * should just return and handle normalized paths without protocol. */
        path = otherpath;
    }

    if (path) {
        path[1] = '\0'; /* remove name after separator */

        strcpy_v(partname, sizeof(partname), filename);
        fix_dir_separators(partname); /* normalize to DIR_SEPARATOR */

        /* normalize relative paths as don't work ok in some plugins */
        if (partname[0] == '.' && partname[1] == DIR_SEPARATOR) { /* './name' */
            name = partname + 2; /* ignore './' */
        }
        else if (partname[0] == '.' && partname[1] == '.' && partname[2] == DIR_SEPARATOR) { /* '../name' */
            char* pathprev;

            path[0] = '\0'; /* remove last separator so next call works */
            pathprev = strrchr(fullname,DIR_SEPARATOR);
            if (pathprev) {
                pathprev[1] = '\0'; /* remove prev dir after separator */
                name = partname + 3; /* ignore '../' */
            }
            else { /* let plugin handle? */
                path[0] = DIR_SEPARATOR;
                name = partname;
            }
            /* could work with more relative paths but whatevs */
        }
        else {
            name = partname;
        }

        strcat_v(fullname, sizeof(fullname), name);
    }
    else {
        strcpy_v(fullname, sizeof(fullname), filename);
    }

    return open_streamfile(sf, fullname);
}

STREAMFILE* open_streamfile_by_filename(STREAMFILE* sf, const char* filename) {
    return open_streamfile_internal(sf, filename, false);
}

STREAMFILE* open_streamfile_by_pathname(STREAMFILE* sf, const char* filename) {
    return open_streamfile_internal(sf, filename, true);
}

/* ************************************************************************* */

int check_extensions(STREAMFILE* sf, const char* cmp_exts) {
    char filename[PATH_LIMIT];
    const char* ext = NULL;
    const char* cmp_ext = NULL;
    const char* ststr_res = NULL;
    size_t ext_len, cmp_len;

    sf->get_name(sf, filename, sizeof(filename));
    ext = filename_extension(filename);
    ext_len = strlen(ext);

    cmp_ext = cmp_exts;
    do {
        ststr_res = strstr(cmp_ext, ",");
        cmp_len = ststr_res == NULL
                  ? strlen(cmp_ext) /* total length if more not found */
                  : (intptr_t)ststr_res - (intptr_t)cmp_ext; /* find next ext; ststr_res should always be greater than cmp_ext, resulting in a positive cmp_len */

        if (ext_len == cmp_len && strncasecmp(ext,cmp_ext, ext_len) == 0)
            return 1;

        cmp_ext = ststr_res;
        if (cmp_ext != NULL)
            cmp_ext = cmp_ext + 1; /* skip comma */

    } while (cmp_ext != NULL);

    return 0;
}

/* ************************************************************************* */

/* copies name as-is (may include full path included) */
void get_streamfile_name(STREAMFILE* sf, char* dst, size_t dst_size) {
    sf->get_name(sf, dst, dst_size);
}

/* copies the filename without path */
void get_streamfile_filename(STREAMFILE* sf, char* dst, size_t dst_size) {
    char foldername[PATH_LIMIT];

    get_streamfile_name(sf, foldername, sizeof(foldername));

    //TODO: Windows CMD accepts both \\ and /, better way to handle this?
    const char* path = strrchr(foldername,'\\');
    if (!path)
        path = strrchr(foldername, '/');
    if (path != NULL)
        path = path + 1;

    if (path) {
        strcpy_v(dst, dst_size, path);
    }
    else {
        strcpy_v(dst, dst_size, foldername);
    }
}

/* copies the filename without path or extension */
void get_streamfile_basename(STREAMFILE* sf, char* dst, size_t dst_size) {

    get_streamfile_filename(sf, dst, dst_size);

    char* ext = strrchr(dst, '.');
    if (ext) {
        ext[0] = '\0'; // remove .ext from buffer
    }
}

/* copies path removing name (NULL when if filename has no path) */
void get_streamfile_path(STREAMFILE* sf, char* dst, size_t dst_size) {

    get_streamfile_name(sf, dst, dst_size);

    const char* path = strrchr(dst,DIR_SEPARATOR);
    if (path != NULL)
        path = path + 1; // includes "/"

    if (path) {
        dst[path - dst] = '\0';
    }
    else {
        dst[0] = '\0';
    }
}

/* copies extension only */
void get_streamfile_ext(STREAMFILE* sf, char* dst, size_t dst_size) {
    char filename[PATH_LIMIT];

    get_streamfile_name(sf, filename, sizeof(filename));
    const char* extension = filename_extension(filename);
    if (!extension) {
        dst[0] = '\n';
    }
    else {
        strcpy_v(dst, dst_size, extension);
    }
}
