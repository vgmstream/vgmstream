#include "string_utils.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Attempt to create usable string functions.
 * 
 * To recap in C we have:
 * - strcpy(dst, src): copies up to src's null-end
 *   - overflows if dst_size < src_size
 * - strncpy(dst, src, dst_size): copies and 0-pads up to dst_size
 *   - doesn't null-terminate if src_size >= dst_size (bad truncation)
 *   - inefficient, if src is only 10, dst_size = 10000 > copies 10 + adds 9990 0s
 * - sprintf(dst, format, ...): copies string based on format
 *   - similar as strcpy
 * - snprintf(dst, dst_size, format, ...): copies string based on format, up to dst_size
 *   - less efficient for simple string copy: parses format + returns strlen(src)
 *   - on truncation null-terminates but returns 'max chars' rather than 'written' (usable but gotcha)
 *   - older <MSVC2015 equivalent _snprintf not 1:1 (different return value and not always null-terminates)
 * - strlcpy(dst, src, dst_size): copies up to dst_size (always null-terminates), 
 *   - not portable (BSD), uncommon param order, returns strlen(src), various criticisms
 * - strncpy_s(dst, dst_size, src, count): copies up to count, returns errno_t + if dst_size is not big enough
 *   - not portable (MS), unwieldly params/return, no truncation
 * - strscpy(dst, src, count): copies up to src null-end OR count, null-terminates, returns copied
 *   - non-standard + ssize_t (Linux kernel)
 *   - on truncation returns -E2BIG (but still copies)
 *   - doesn't read src past count on truncation (good)
 * - memcpy(dst, src, count): copies up to count, no null-termination
 *   - easy to mess up counts, not an string function
 * (same for *cat family)
 * Returning values always(?) exclude null-terminator. Truncation is not UTF-8 aware (leaves wrong clusters).
 *
 * snprintf with "%s" is the most usable one, but less efficient (format parsing, src may be huge/untrusted)
 * and return value is easy to misuse.
 * Having home-baked stuff for everyday cases isn't desirable but for now have some strscpy-like functions
 * that are more predictable.
 */



int strcpy_v(char* dst, size_t dst_size, const char* src) {
    if (dst == NULL || dst_size == 0)
        return 0;

    if (src == NULL) {
        dst[0] = '\0';
        return -1;
    }

    // find src max but don't read past limit (strnlen is similar but not portable)
    // could do a copy_len pass then use memcpy (maybe faster for bigger strings), but not sure
    size_t copy_len = 0;
    size_t copy_max = dst_size - 1; // for null
    while (copy_len < copy_max && src[copy_len] != '\0') {
        dst[copy_len] = src[copy_len];
        copy_len++;
    }
    dst[copy_len] = '\0';

    // truncation
    if (copy_len == copy_max && src[copy_len] != '\0')
        return -1;

    return (int)copy_len;
}


int strcat_v(char* dst, size_t dst_size, const char* src) {
    if (dst == NULL || dst_size == 0)
        return 0;

    if (src == NULL)
        return -1;

    // find dst max, up to dst_size
    size_t dst_len = 0;
    while (dst_len < dst_size && dst[dst_len] != '\0') {
        dst_len++;
    }

    if (dst_len == dst_size) {
        dst[dst_size - 1] = '\0';
        return -1;
    }

    if (src == NULL) {
        dst[dst_len] = '\0';
        return -1;
    }

    // copy rest
    return strcpy_v(dst + dst_len, dst_size - dst_len, src);
}

#if 0
int strcpy_v(char* dst, size_t dst_max, const char* src) {
    if (!dst || !src || dst_max == 0)
        return -1;

    int n = snprintf(dst, dst_max, "%s", src);
    return n;
}

int strcat_(char* dst, size_t dst_max, const char* src) {
    if (!dst || !src || dst_max == 0)
        return -1;

    size_t dst_len = strlen(dst);
    if (dst_len >= dst_max) // to substraction issues
        return -1;

    int n = snprintf(dst + dst_len, dst_max - dst_len, "%s", src);
    return (int)dst_len + n;
}
#endif


void str_lowercase_v(char* dst, size_t dst_size) {
    if (dst == NULL || dst_size == 0)
        return;

    size_t dst_len = 0;
    while (dst_len < dst_size && dst[dst_len] != '\0') {
        dst[dst_len] = (char)tolower((unsigned char)dst[dst_len]);
        dst_len++;
    }

    if (dst_len == dst_size) {
        dst[dst_size - 1] = '\0';
    }
}

void str_uppercase_v(char* dst, size_t dst_size) {
    if (dst == NULL || dst_size == 0)
        return;

    size_t dst_len = 0;
    while (dst_len < dst_size && dst[dst_len] != '\0') {
        dst[dst_len] = (char)toupper((unsigned char)dst[dst_len]);
        dst_len++;
    }

    if (dst_len == dst_size) {
        dst[dst_size - 1] = '\0';
    }
}


bool str_is_uppercase(const char* str) {
    if (str == NULL || str[0] == '\0')
        return false;

    while (str[0] != '\0') {
        char c = str[0];
        if (c < 'A' || c > 'Z')
            return false;
        str++;
    }

    return true;
}
