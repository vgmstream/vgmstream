/**
 * log for Winamp
 */
#include "in_vgmstream.h"

/* could just write to file but to avoid leaving temp crap just log to memory and print what when requested */

winamp_log_t* walog;
#define WALOG_MAX_LINES 32
#define WALOG_MAX_CHARS 256

struct winamp_log_t {
    char data[WALOG_MAX_LINES * WALOG_MAX_CHARS];
    int logged;
    const char* lines[WALOG_MAX_LINES];
};

void logger_init() {
    walog = malloc(sizeof(winamp_log_t));
    if (!walog) return;

    walog->logged = 0;
}

void logger_free() {
    free(walog);
    walog = NULL;
}

/* logs to data as a sort of circular buffer. example if max_lines is 6:
 * - log 0 = "msg1"
 * ...
 * - log 5 = "msg5" > limit reached, next will overwrite 0
 * - log 0 = "msg6" (max 6 logs, but can only write las 6)
 * - when requested lines should go from current to: 1,2,3,4,5,0
*/
void logger_callback(int level, const char* str) {
    char* buf;
    int pos;
    if (!walog)
        return;

    pos = (walog->logged % WALOG_MAX_LINES) * WALOG_MAX_CHARS;
    buf = &walog->data[pos];
    snprintf(buf, WALOG_MAX_CHARS, "%s", str);

    walog->logged++;

    /* ??? */
    if (walog->logged >= 0x7FFFFFFF)
        walog->logged = 0;
}

const char** logger_get_lines(int* p_max) {
    int i, from, max;

    if (!walog) {
        *p_max = 0;
        return NULL;
    }

    if (walog->logged > WALOG_MAX_LINES) {
        from = (walog->logged % WALOG_MAX_LINES);
        max = WALOG_MAX_LINES;
    }
    else {
        from = 0;
        max = walog->logged;
    }

    for (i = 0; i < max; i++) {
        int pos = ((from + i) % WALOG_MAX_LINES) * WALOG_MAX_CHARS;
        walog->lines[i] = &walog->data[pos];
    }

    *p_max = max;
    return walog->lines;
}
