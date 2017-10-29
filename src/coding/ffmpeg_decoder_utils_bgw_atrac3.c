#if 1
#include "coding.h"
#include "ffmpeg_decoder_utils.h"

#ifdef VGM_USE_FFMPEG

#define BGM_ATRAC3_FRAME_SIZE  0xC0

/**
 * Encrypted ATRAC3 used in BGW (Final Fantasy XI PC).
 * Info from Moogle Toolbox: https://sourceforge.net/projects/mogbox/
 */

int ffmpeg_custom_read_bgw_atrac3(ffmpeg_codec_data *data, uint8_t *buf, int buf_size) {
    int i, ch;
    size_t bytes;
    size_t block_align = BGM_ATRAC3_FRAME_SIZE * data->config.channels;


    /* init key: first frame + modified channel header */
    if (data->config.key == NULL) {
        data->config.key = malloc(block_align);
        if (!data->config.key) return 0;

        read_streamfile(data->config.key, data->real_start, block_align, data->streamfile);
        for (ch = 0; ch < data->config.channels; ch++) {
            uint32_t xor = get_32bitBE(data->config.key + ch*BGM_ATRAC3_FRAME_SIZE);
            put_32bitBE(data->config.key + ch*BGM_ATRAC3_FRAME_SIZE, xor ^ 0xA0024E9F);
        }
    }


    /* read normally and unXOR the data */
    bytes = read_streamfile(buf, data->real_offset, buf_size, data->streamfile);
    for (i = 0; i < bytes; i++) {
        int key_pos = (data->real_offset - data->real_start + i) % block_align;
        buf[i] = buf[i] ^ data->config.key[key_pos];
    }


    data->real_offset += bytes;
    return bytes;
}

int64_t ffmpeg_custom_seek_bgw_atrac3(ffmpeg_codec_data *data, int64_t virtual_offset) {
    int64_t seek_virtual_offset = virtual_offset - data->header_size;

    data->real_offset = data->real_start + seek_virtual_offset;
    return virtual_offset;
}

int64_t ffmpeg_custom_size_bgw_atrac3(ffmpeg_codec_data *data) {
    return data->real_size + data->header_size;
}


#endif
#endif
