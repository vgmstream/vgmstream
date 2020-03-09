#ifndef _XNB_STREAMFILE_H_
#define _XNB_STREAMFILE_H_

//#define XNB_ENABLE_LZX 1

#ifdef XNB_ENABLE_LZX
/* lib from https://github.com/sumatrapdfreader/chmlib
 * which is a cleaned-up version of https://github.com/jedwing/CHMLib */
#include "lzx.h"

#define LZX_XNB_WINDOW_BITS 16
#endif

#include "xnb_lz4mg.h"


#define XNB_TYPE_LZX  1
#define XNB_TYPE_LZ4  2

typedef struct {
    /* config */
    int type;
    off_t compression_start;
    size_t compression_size;

    /* state */
    off_t logical_offset;   /* offset that corresponds to physical_offset */
    off_t physical_offset;  /* actual file offset */

    size_t block_size;      /* current block size */
    size_t skip_size;       /* size to skip from a block start to reach data start */
    size_t data_size;       /* logical size of the block */

    size_t logical_size;

    /* decompression state (dst size min for LZX) */
    uint8_t dst[0x10000];
    uint8_t src[0x10000];

    lz4mg_stream_t lz4s;
#ifdef XNB_ENABLE_LZX
    struct lzx_state* lzxs;
#endif
} xnb_io_data;

static int xnb_io_init(STREAMFILE* sf, xnb_io_data* data) {
    /* When a new IO SF is opened, the data struct is malloc'd and cloned. This works
     * well enough in other cases, but b/c the decompression context works with buf ptrs
     * the clone will point to the original struct bufs, so we need to force
     * reset on new open so that new bufs of the clone are used. */
    data->logical_offset = -1;

#ifdef XNB_ENABLE_LZX
    if (data->type == XNB_TYPE_LZX) {
        data->lzxs = lzx_init(LZX_XNB_WINDOW_BITS);
        if (!data->lzxs)
            return -1;
    }
#endif

    return 0;
}

static void xnb_io_close(STREAMFILE* sf, xnb_io_data* data) {
#ifdef XNB_ENABLE_LZX
    if (data->type == XNB_TYPE_LZX) {
        lzx_teardown(data->lzxs);
    }
#endif
}

#ifdef XNB_ENABLE_LZX
/* Decompresses LZX used in XNB. Has 16b window and headered blocks (with input size and
 * optionally output size) and standard LZX inside, probably what's known as XMemCompress.
 * Info: https://github.com/MonoGame/MonoGame/blob/develop/MonoGame.Framework/Utilities/LzxStream/LzxDecoderStream.cs */
static int decompress_lzx_block(STREAMFILE* sf, xnb_io_data* data) {
    int src_size, dst_size, ret;
    uint8_t hi, lo;
    off_t head_size;
    off_t offset = data->physical_offset;


    hi = read_u8(offset + 0x00, sf);
    if (hi == 0xFF) {
        hi = read_u8(offset + 0x01, sf);
        lo = read_u8(offset + 0x02, sf);
        dst_size = (hi << 8) | lo;

        hi = read_u8(offset + 0x03, sf);
        lo = read_u8(offset + 0x04, sf);
        src_size = (hi << 8) | lo;

        head_size = 0x05;
    }
    else {
        dst_size = 0x8000; /* default */

        lo = read_u8(offset + 0x01, sf);
        src_size = (hi << 8) | lo;

        head_size = 0x02;
    }

    if (src_size == 0 || dst_size == 0) {
        VGM_LOG("LZX: EOF\n");
        return 1;
    }

    read_streamfile(data->src, offset + head_size, src_size, sf);

    ret = lzx_decompress(data->lzxs, data->src, data->dst, src_size, dst_size);
    if (ret != DECR_OK) {
        VGM_LOG("LZX: decompression error %i\n", ret);
        goto fail;
    }

    data->data_size = dst_size;
    data->block_size = head_size + src_size;

    return 1;
fail:
    return 0;
}
#endif

static int decompress_lz4_block(STREAMFILE* sf, xnb_io_data* data) {
    int ret;

    if (data->lz4s.avail_in == 0) {
        data->lz4s.next_in = data->src;
        data->lz4s.avail_in = read_streamfile(data->src, data->physical_offset, sizeof(data->src), sf);

        /* shouldn't happen since we have decomp size */
        if (data->lz4s.avail_in <= 0) {
            VGM_LOG("XNB: EOF reached\n");
            goto fail;
        }

        data->block_size = data->lz4s.avail_in; /* physical data (smaller) */
    }
    else {
        data->block_size = 0; /* physical data doesn't move until new read */
    }

    data->lz4s.total_out = 0;
    data->lz4s.avail_out = sizeof(data->dst);
    data->lz4s.next_out = data->dst;

    ret = lz4mg_decompress(&data->lz4s);

    data->data_size = data->lz4s.total_out; /* logical data (bigger) */

    if (ret != LZ4MG_OK) {
        VGM_LOG("XNB: LZ4 error %i\n", ret);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static size_t xnb_io_read(STREAMFILE* sf, uint8_t *dest, off_t offset, size_t length, xnb_io_data* data) {
    size_t total_read = 0;

    /* reset */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = 0x00;
        data->logical_offset = 0x00;
        data->block_size = 0;
        data->data_size = 0;
        data->skip_size = 0;

        switch(data->type) {
#ifdef XNB_ENABLE_LZX
            case XNB_TYPE_LZX: lzx_reset(data->lzxs); break;
#endif
            case XNB_TYPE_LZ4: lz4mg_reset(&data->lz4s); break;
            default: break;
        }
    }

    /* read blocks, one at a time */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 || data->logical_offset >= data->logical_size) {
            break;
        }

        /* process new block */
        if (data->data_size <= 0) {

            if (data->physical_offset < data->compression_start) {
                /* copy data */
                int to_read = data->compression_start - data->physical_offset;

                data->data_size = read_streamfile(data->dst, data->physical_offset, to_read, sf);
                data->block_size = data->data_size;
            }
            else {
                /* decompress data */
                int ok = 0;

                switch(data->type) {
#ifdef XNB_ENABLE_LZX
                    case XNB_TYPE_LZX: ok = decompress_lzx_block(sf, data); break;
#endif
                    case XNB_TYPE_LZ4: ok = decompress_lz4_block(sf, data); break;
                    default: break;
                }
                if (!ok) {
                    VGM_LOG("XNB: decompression error\n");
                    break;
                }
            }
        }

        /* move to next block */
        if (data->data_size == 0 || offset >= data->logical_offset + data->data_size) {
            data->physical_offset += data->block_size;
            data->logical_offset += data->data_size;
            data->data_size = 0;
            continue;
        }

        /* read/copy block data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;
            to_read = data->data_size - bytes_consumed;
            if (to_read > length)
                to_read = length;
            memcpy(dest, data->dst + data->skip_size + bytes_consumed, to_read);
            bytes_done = to_read;

            total_read += bytes_done;
            dest += bytes_done;
            offset += bytes_done;
            length -= bytes_done;

            if (bytes_done != to_read || bytes_done == 0) {
                break; /* error/EOF */
            }
        }

    }

    return total_read;
}


static size_t xnb_io_size(STREAMFILE *streamfile, xnb_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    xnb_io_read(streamfile, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}


/* Decompresses XNB streams */
static STREAMFILE* setup_xnb_streamfile(STREAMFILE* sf, int flags, off_t compression_start, size_t compression_size) {
    STREAMFILE *new_sf = NULL;
    xnb_io_data io_data = {0};

    if (flags & 0x80)
        io_data.type = XNB_TYPE_LZX;
    else if (flags & 0x40)
        io_data.type = XNB_TYPE_LZ4;
    else
        goto fail;
    io_data.compression_start = compression_start;
    io_data.compression_size = compression_size;
    io_data.physical_offset = 0x00;
    io_data.logical_size = compression_start + compression_size;
    io_data.logical_offset = -1; /* force reset */

#ifndef XNB_ENABLE_LZX
    /* no known audio use it (otherwise works if enabled and included lzx.c/lzx.h) */
    if (io_data.type == XNB_TYPE_LZX)
        goto fail;
#endif

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_ex_f(new_sf, &io_data, sizeof(xnb_io_data), xnb_io_read, xnb_io_size, xnb_io_init, xnb_io_close);
    //new_sf = open_buffer_streamfile_f(new_sf, 0); /* useful? we already have a decompression buffer */
    return new_sf;
fail:
    return NULL;
}

#endif /* _XNB_STREAMFILE_H_ */
