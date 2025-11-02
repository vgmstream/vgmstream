#include <string.h>
#include "util.h"
#include "streamtypes.h"

const char* filename_extension(const char* pathname) {
    const char* extension;

    /* favor strrchr (optimized/aligned) rather than homemade loops */
    extension = strrchr(pathname,'.');

    if (extension != NULL) {
        /* probably has extension */
        extension++; /* skip dot */

        /* find possible separators to avoid misdetecting folders with dots + extensionless files
         * (after the above to reduce search space, allows both slashes in case of non-normalized names) */
        if (strchr(extension, '/') == NULL && strchr(extension, '\\') == NULL)
            return extension; /* no slashes = really has extension */
    }

    /* extensionless: point to null after current name 
     * (could return NULL but prev code expects with to return an actual c-string) */
    return pathname + strlen(pathname);
}


/* math helpers */
uint32_t clamp_u32(uint32_t v, uint32_t min, uint32_t max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

size_t align_size_to_block(size_t value, size_t block_align) {
    if (!block_align)
        return 0;

    size_t extra_size = value % block_align;
    if (extra_size == 0) return value;
    return (value + block_align - extra_size);
}

/* length is maximum length of dst. dst will always be null-terminated if
 * length > 0 */
void concatn(int length, char * dst, const char * src) {
    int i,j;
    if (length <= 0) return;
    for (i=0;i<length-1 && dst[i];i++);   /* find end of dst */
    for (j=0;i<length-1 && src[j];i++,j++)
        dst[i]=src[j];
    dst[i]='\0';
}

bool check_subsongs(int* target_subsong, int total_subsongs) {
    if (*target_subsong == 0)
        *target_subsong = 1;
    if (*target_subsong < 0 || *target_subsong > total_subsongs || total_subsongs < 1)
        return false;
    return true;
}