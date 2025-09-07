#ifndef _VJSON_H_
#define _VJSON_H_

/* What is this crap, you may wonder? For probably non-existant use cases Jansson was added to write JSON info,
 * but external libs are a pain to maintain. For now this glorified string joiner replaces it.
 *
 * On incorrect usage or small buf it'll create invalid JSON because who cares, try-parse-catch as usual.
 *
 * Example usage:
 *     char buf[MAX_JSON_SIZE];                     // fixed size, meaning we need to know the approximate max
 *     vjson_t j = {0};                             // alloc this or the buf if needed
 *     vjson_init(&j, buf, sizeof(buf));            // prepare writer
 *
 *     vjson_obj_open(&j);                          // new object {...}
 *       vjson_keystr(&j, "key-str", str_value);    // add 'key: "value"' to current object
 *       vjson_keyint(&j, "key-int", int_value);    // add 'key: value' to current object
 *       vjson_key(&j, "key");                      // add 'key: ' (for objects or arrays)
 *         vjson_arr_open(&j);                      // new array [...]
 *           vjson_str(&j, str_value);              // add '"value"' to current array
 *           vjson_int(&j, int_value);              // add 'value' to current array
 *         vjson_arr_close(&j);                     // close current array
 *     vjson_obj_close(&j);                         // close current object
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

#define VJSON_STACK_MAX 16
typedef struct {
    char* buf;
    int buf_len;
    char* bufp;
    int buf_left;

    bool stack[VJSON_STACK_MAX];
    int stack_pos;
    bool is_last_key;
} vjson_t;

static void vjson_init(vjson_t* j, char* buf, int buf_len) {
    j->buf = buf;
    j->buf_len = buf_len;
    j->bufp = buf;
    j->buf_left = buf_len;
}

static void vjson_raw(vjson_t* j, const char* str) {
    if (!str)
        str = "null";
    int done = snprintf(j->bufp, j->buf_left, "%s", str);

    j->bufp += done;
    j->buf_left -= done;
}

static void vjson_comma_(vjson_t* j) {
    if (j->stack[j->stack_pos] && !j->is_last_key) {
        vjson_raw(j, ",");
    }
    j->stack[j->stack_pos] = true;
    j->is_last_key = false;
}

static void vjson_open_(vjson_t* j, const char* str) {
    vjson_comma_(j);
    vjson_raw(j, str);

    if (j->stack_pos + 1 >= VJSON_STACK_MAX)
        return;
    j->stack_pos++;
    j->stack[j->stack_pos] = false;
}

static void vjson_close_(vjson_t* j, const char* str) {
    //vjson_comma_(j);
    vjson_raw(j, str);

    if (j->stack_pos - 1 <= 0)
        return;
    j->stack[j->stack_pos] = false;
    j->stack_pos--;
}

static void vjson_arr_open(vjson_t* j) {
    vjson_open_(j, "[");
}

static void vjson_arr_close(vjson_t* j) {
    vjson_close_(j, "]");
}

static void vjson_obj_open(vjson_t* j) {
    vjson_open_(j, "{");
}

static void vjson_obj_close(vjson_t* j) {
    vjson_close_(j, "}");
}

static void vjson_key(vjson_t* j, const char* key) {
    vjson_comma_(j);
    vjson_raw(j, "\"");
    vjson_raw(j, key);
    vjson_raw(j, "\":");
    j->is_last_key = true;
}

static void vjson_str(vjson_t* j, const char* str) {
    vjson_comma_(j);
    if (!str || str[0] == '\0') {
        vjson_raw(j, NULL);
    }
    else {
        vjson_raw(j, "\"");
        vjson_raw(j, str);
        vjson_raw(j, "\"");
    }
}

static void vjson_int(vjson_t* j, int64_t num) {
    vjson_comma_(j);

    char tmp[32] = {0};
    snprintf(tmp, sizeof(tmp), "%" PRId64, num);
    vjson_raw(j, tmp);
}

#if 0
static void vjson_dbl(vjson_t* j, double num) {
    vjson_comma_(j);

    char tmp[32] = {0};
    snprintf(tmp, sizeof(tmp), "%f", num);
    vjson_raw(j, tmp);
}
#endif

static void vjson_null(vjson_t* j){
    vjson_comma_(j);
    vjson_raw(j, "null");
}

static void vjson_intnull(vjson_t* j, int64_t num) {
    if (num == 0) {
        vjson_str(j, NULL);
    }
    else {
        vjson_int(j, num);
    }
}

static void vjson_keystr(vjson_t* j, const char* key, const char* val) {
    vjson_key(j, key);
    vjson_str(j, val);
}

static void vjson_keyint(vjson_t* j, const char* key, int64_t val) {
    vjson_key(j, key);
    vjson_int(j, val);
}

static void vjson_keyintnull(vjson_t* j, const char* key, int64_t val) {
    vjson_key(j, key);
    vjson_intnull(j, val);
}

#endif
