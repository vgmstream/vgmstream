#include "coding.h"
#include "ffmpeg_decoder_utils.h"

#ifdef VGM_USE_FFMPEG

#define EAXMA_XMA_BLOCK_SIZE 0x800

/**
 * EA-XMA is XMA with padding removed (so a real 0x450 block would be padded to a virtual 0x800 block).
 * //todo missing multichannel (packet multistream) support, unknown layout
 */


int ffmpeg_custom_read_eaxma(ffmpeg_codec_data *data, uint8_t *buf, int buf_size) {
    uint8_t v_buf[0x8000]; /* intermediate buffer, could be simplified */
    int buf_done = 0;
    uint64_t real_offset = data->real_offset;
    uint64_t virtual_offset = data->virtual_offset - data->header_size;
    uint64_t virtual_base = data->virtual_base;


    /* read and transform SNS/EA-XMA block into XMA block by adding padding */
    while (buf_done < buf_size) {
        int bytes_to_copy;
        size_t data_size, extra_size = 0, gap_size = 0;
        size_t block_size = read_32bitBE(real_offset, data->streamfile);
        /* 0x04(4): some kind of size? 0x08(4): decoded samples */

        /* setup */
        data_size = (block_size & 0x00FFFFFF) - 0x0c; //todo last block size may be slightly off?
        if (data_size % EAXMA_XMA_BLOCK_SIZE) /* aligned padding */
            extra_size = EAXMA_XMA_BLOCK_SIZE - (data_size % EAXMA_XMA_BLOCK_SIZE);
        if (buf_done == 0) /* first read */
            gap_size = virtual_offset - virtual_base; /* might start a few bytes into the block */

        if (data_size + extra_size > 0x8000) {
            VGM_LOG("EA-XMA: total size bigger than buffer at %lx\n", (off_t)real_offset);
            return 0;
        }

        bytes_to_copy = data_size + extra_size - gap_size;
        if (bytes_to_copy > buf_size - buf_done)
            bytes_to_copy = buf_size - buf_done;

        /* transform */
        read_streamfile(v_buf, real_offset + 0x0c, data_size, data->streamfile);
        memset(v_buf + data_size, 0xFF, extra_size); /* padding can be any value, typically 0xFF */
        memcpy(buf + buf_done, v_buf + gap_size, bytes_to_copy);

        /* move when block is fully done */
        if (data_size + extra_size == bytes_to_copy + gap_size) {
            real_offset += (block_size & 0x00FFFFFF);
            virtual_base += data_size + extra_size;
        }

        buf_done += bytes_to_copy;

        /* exit on last block just in case, though should reach file size */
        if (block_size & 0x80000000)
            break;
    }


    data->real_offset = real_offset;
    data->virtual_base = virtual_base;
    return buf_size;
}

int64_t ffmpeg_custom_seek_eaxma(ffmpeg_codec_data *data, int64_t virtual_offset) {
    int64_t real_offset, virtual_base;
    int64_t current_virtual_offset = data->virtual_offset;

    /* Find SNS block start closest to offset. ie. virtual_offset 0x1A10 could mean SNS blocks
     * of 0x456+0x820 padded to 0x800+0x1000 (base) + 0x210 (extra for reads), thus real_offset = 0xC76 */

    if (virtual_offset > current_virtual_offset) { /* seek after current: start from current block */
        real_offset = data->real_offset;
        virtual_base = data->virtual_base;
    }
    else { /* seek before current: start from the beginning */
        real_offset = data->real_start;
        virtual_base = 0;
    }


    /* find target block */
    while (virtual_base < virtual_offset) {
        size_t data_size, extra_size = 0;
        size_t block_size = read_32bitBE(real_offset, data->streamfile);

        data_size = (block_size & 0x00FFFFFF) - 0x0c;
        if (data_size % EAXMA_XMA_BLOCK_SIZE)
            extra_size = EAXMA_XMA_BLOCK_SIZE - (data_size % EAXMA_XMA_BLOCK_SIZE);

        /* stop if virtual_offset lands inside current block */
        if (data_size + extra_size > virtual_offset)
            break;

        real_offset += (block_size & 0x00FFFFFF);
        virtual_base += data_size + extra_size;
    }

    /* closest we can use for reads */
    data->real_offset = real_offset;
    data->virtual_base = virtual_base;

    return virtual_offset;
}

int64_t ffmpeg_custom_size_eaxma(ffmpeg_codec_data *data) {

    uint64_t virtual_size = data->config.virtual_size;
    if (!virtual_size)
        return 0;

    return virtual_size + data->header_size;
}

/* needed to know in meta for fake RIFF */
size_t ffmpeg_get_eaxma_virtual_size(off_t real_offset, size_t real_size, STREAMFILE *streamFile) {
    size_t virtual_size = 0;

    /* count all SNS/EAXMA blocks size + padding size */
    while (real_offset < real_size) {
        size_t data_size;
        size_t block_size = read_32bitBE(real_offset, streamFile);

        data_size = (block_size & 0x00FFFFFF) - 0x0c;

        if ((block_size & 0xFF000000) && !(block_size & 0x80000000)) {
            VGM_LOG("EA-XMA: unknown flag found at %lx\n", (off_t)real_offset);
            goto fail;
        }

        real_offset += (block_size & 0x00FFFFFF);

        virtual_size += data_size;
        if (data_size % EAXMA_XMA_BLOCK_SIZE) /* XMA block padding */
            virtual_size += EAXMA_XMA_BLOCK_SIZE - (data_size % EAXMA_XMA_BLOCK_SIZE);

        /* exit on last block just in case, though should reach real_size */
        if (block_size & 0x80000000)
            break;
    }

    return virtual_size;

fail:
    return 0;

}

#endif
