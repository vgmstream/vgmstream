#include "coding.h"
#include "ffmpeg_decoder_utils.h"

#ifdef VGM_USE_FFMPEG

/**
 * Standard read mode: virtual values are 1:1 but inside a portion of the streamfile (between real_start and real_size).
 */


int ffmpeg_custom_read_standard(ffmpeg_codec_data *data, uint8_t *buf, int buf_size) {
    size_t bytes = read_streamfile(buf, data->real_offset, buf_size, data->streamfile);
    data->real_offset += bytes;

    return bytes;
}

int64_t ffmpeg_custom_seek_standard(ffmpeg_codec_data *data, int64_t virtual_offset) {
    int64_t seek_virtual_offset = virtual_offset - data->header_size;

    data->real_offset = data->real_start + seek_virtual_offset;
    return virtual_offset;
}

int64_t ffmpeg_custom_size_standard(ffmpeg_codec_data *data) {
    return data->real_size + data->header_size;
}


#endif
