#include "sf_utils.h"
#include "../vgmstream.h"
#include "reader_sf.h"
#include "paths.h"
#include "log.h"
#include "string_utils.h"


STREAMFILE* open_streamfile_by_ext(STREAMFILE* sf, const char* ext) {
    char filename[PATH_LIMIT];

    get_streamfile_name(sf, filename, sizeof(filename));

    swap_extension(filename, sizeof(filename), ext);

    return open_streamfile(sf, filename);
}

static STREAMFILE* open_streamfile_internal(STREAMFILE* sf, const char* filename, bool allow_relpaths) {
    char fullname[PATH_LIMIT];

    if (!sf || !filename || filename[0] == '\0')
        return NULL;

    // Some formats open companion files in current dir or subfolders. Relative paths are restricted by default
    // for better control access. Mainly for doc purposes (most formats wouldn't need that), since
    // the security risk of a format opening an arbitrary file is low given regular .m3u allow arbitrary paths
    // anyway, and without RCE it can't leave your system (and web player can't read the filesystem).
    if (!allow_relpaths && is_restricted_path(filename)) {
        VGM_LOG("SF: ignored relative path %s\n", filename);
        return NULL;
    }

    char* path_end; // points to last slash
    char separator;

    /* prepare final name and detect separator + last slash */
    get_streamfile_name(sf, fullname, sizeof(fullname));
    get_path_info(fullname, &path_end, &separator);

    if (path_end) {
        // original filename has a full path
        path_end[1] = '\0'; // remove name after separator so it can be concat'd
        size_t path_len = (int)(path_end - fullname);

        const char* name;        
        if (filename[0] == '.' && (filename[1] == '\\' || filename[1] == '/')) {
            // "./name": skip relative paths some plugins don't like them
            name = filename + 2;
        }
        #if 0
        else if (filename[0] == '.' && filename[1] == '.' && (filename[2] == '\\' || filename[2] == '/')) {
            // '../name': could try to go back, but relative paths may be N levels deep
            ...
        }
        #endif
        else {
            // others (ex. "name", "subdir/name", "../../name", etc): assume plugin can handle them
            name = filename;
        }

        strcat_v(fullname, sizeof(fullname), name);
        // concat'd name only
        normalize_path(fullname + path_len, sizeof(fullname) - path_len, separator);

    }
    else {
        // original filename has no path (for on CLI; plugins should have a full-ish path)
        strcpy_v(fullname, sizeof(fullname), filename);
        // use OS default, just in case
        normalize_path(fullname, sizeof(fullname), 0);
    }

    return open_streamfile(sf, fullname);
}

STREAMFILE* open_streamfile_by_filename(STREAMFILE* sf, const char* filename) {
    return open_streamfile_internal(sf, filename, false);
}

STREAMFILE* open_streamfile_by_pathname(STREAMFILE* sf, const char* filename) {
    return open_streamfile_internal(sf, filename, true);
}

STREAMFILE* open_streamfile_by_absname(STREAMFILE* sf, const char* filename) {
    /* absolute paths are detected for convenience, but since it's hard to unify all OSs
     * and plugins, they aren't "officially" supported nor documented, thus may or may not work */
    if (is_path_absolute(filename)) {
        return open_streamfile(sf, filename); // from path as is
    }
    else {
        return open_streamfile_by_pathname(sf, filename); // from current path
    }
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

bool check_file_size(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size) {
    if (sf == NULL)
        return false;

    uint32_t sf_size = get_streamfile_size(sf);
    // overflow-safe checks
    return sf_size >= data_offset && data_size == sf_size - data_offset;
}


/* ************************************************************************* */

/* copies name as-is (may include full path) */
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

#if 0 //untested
/* copies path removing name (NULL when if filename has no path) */
void get_streamfile_path(STREAMFILE* sf, char* dst, size_t dst_size) {

    char* path_end; // points to last slash
    char separator;

    /* prepare final name and detect separator + last slash */
    get_streamfile_name(sf, dst, dst_size);
    get_path_info(dst, &path_end, &separator);

    const char* path = strrchr(dst,DIR_SEPARATOR);
    if (path != NULL)
        path = path + 1; // includes "/"

    if (path) {
        path_end[1] = '\0'; // remove name after separator
    }
    else {
        dst[0] = '\0';
    }
}
#endif

/* copies extension only */
void get_streamfile_ext(STREAMFILE* sf, char* dst, size_t dst_size) {
    char filename[PATH_LIMIT];

    get_streamfile_name(sf, filename, sizeof(filename));
    const char* extension = filename_extension(filename);
    if (!extension) {
        dst[0] = '\0';
    }
    else {
        strcpy_v(dst, dst_size, extension);
    }
}
