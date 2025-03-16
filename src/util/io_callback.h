#ifndef _IO_CALLBACK_H_
#define _IO_CALLBACK_H_

#include <stdint.h>

#define IO_CALLBACK_SEEK_SET  0
#define IO_CALLBACK_SEEK_CUR  1
#define IO_CALLBACK_SEEK_END  2
typedef struct {
    void* arg;
    int64_t (*read)(void* dst, int size, int n, void* arg);
    int64_t (*seek)(void* arg, int64_t offset, int whence);
    int64_t (*tell)(void* arg);
} io_callback_t;

#endif
