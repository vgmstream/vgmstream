#ifndef _DECODE_STATE_H
#define _DECODE_STATE_H

#include "sbuf.h"

typedef struct {
    int discard;
    sbuf_t sbuf;
    int samples_left; //info for some decoders
    int samples_into;
} decode_state_t;

#endif
