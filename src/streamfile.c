#include "streamfile.h"
#include "util/vgmstream_limits.h"
#include "util/sf_utils.h"


STREAMFILE* open_streamfile(STREAMFILE* sf, const char* pathname) {
    return sf->open(sf, pathname, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE* reopen_streamfile(STREAMFILE* sf, size_t buffer_size) {
    char pathname[PATH_LIMIT];

    if (!sf) return NULL;

    if (buffer_size == 0)
        buffer_size = STREAMFILE_DEFAULT_BUFFER_SIZE;
    get_streamfile_name(sf, pathname, sizeof(pathname));
    return sf->open(sf, pathname, buffer_size);
}
