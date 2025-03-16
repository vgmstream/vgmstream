#ifndef _IO_CALLBACK_SF_H_
#define _IO_CALLBACK_SF_H_

#include "io_callback.h"
#include "../streamfile.h"
#include <stdint.h>
#include <stdio.h>

typedef struct {
    STREAMFILE* sf;
    int64_t offset;
} io_priv_t;

void io_callbacks_set_sf(io_callback_t* cb, io_priv_t* arg);

#endif
