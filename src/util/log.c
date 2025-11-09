#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

/* log context; should probably make a unique instance and pass to metas/decoders/etc, but for the time being use a global */
//extern ...* log;

typedef struct {
    int level;
    void (*callback)(int level, const char* str);
} logger_t;

logger_t log_impl = {0};
//void* log = &log_impl;

enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_DEBUG = 2,
    LOG_LEVEL_ALL = 100,
};

static void vgm_log_callback_printf(int level, const char* str) {
    printf("%s", str);
}

void vgm_log_set_callback(void* ctx_p, int level, int type, void* callback) {
    logger_t* ctx = ctx_p;
    if (!ctx) ctx = &log_impl;

    ctx->level = level;

    if (level == LOG_LEVEL_NONE) {
        ctx->callback = NULL;
        return;
    }

    switch(type) {
        case 0:
            ctx->callback = callback;
            break;
        case 1:
            ctx->callback = vgm_log_callback_printf;
            break;
        default:
            break;
    }
}

#if defined(VGM_LOG_OUTPUT) || defined(VGM_DEBUG_OUTPUT)
static void log_internal(void* ctx_p, int level, const char* fmt, va_list args) {
    logger_t* ctx = ctx_p;
    if (!ctx) ctx = &log_impl;

    if (!ctx->callback)
        return;

    if (level > ctx->level)
        return;

    char line[256];
    int done = vsnprintf(line, sizeof(line), fmt, args);
    if (done < 0 || done > sizeof(line))
        strcpy(line, "(ignored log)"); //to-do something better, meh
    ctx->callback(level, line);
}
#endif

#ifdef VGM_DEBUG_OUTPUT
void vgm_logd(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_internal(NULL, LOG_LEVEL_DEBUG, fmt, args);
    va_end(args);
}
#endif

#if defined(VGM_LOG_OUTPUT) || defined(VGM_DEBUG_OUTPUT)
void vgm_logi(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_internal(NULL, LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

void vgm_asserti(int condition, const char* fmt, ...) {
    if (!condition)
        return;

    {
        va_list args;

        va_start(args, fmt);
        log_internal(NULL, LOG_LEVEL_INFO, fmt, args);
        va_end(args);
    }
}
#endif
