#include <ctype.h>
#include "hashes.h"
#include "sf_utils.h"
#include "vgmstream_limits.h"

/* our favorite garbo hash a.k.a FNV-1 32b */
uint32_t hash_str_lc(const char* str) {

    uint32_t hash = 2166136261;
    int i = 0;
    while (str[i] != '\0') {
        char c = tolower(str[i]);
        hash = (hash * 16777619) ^ (uint8_t)c;
        i++;
    }

    return hash;
}

uint32_t hash_sf(STREAMFILE* sf) {
    char path[PATH_LIMIT];

    get_streamfile_name(sf, path, sizeof(path));

    return hash_str_lc(path);
}
