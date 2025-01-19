#ifndef _DECODE_STATE_H
#define _DECODE_STATE_H

#include "sbuf.h"

typedef struct {
    int discard;
    sbuf_t sbuf;
} decode_state_t;

#endif
