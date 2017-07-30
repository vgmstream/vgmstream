#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG
#define MPEG_AHX_EXPECTED_FRAME_SIZE 0x414


/* writes data to the buffer and moves offsets, transforming AHX frames as needed */
int mpeg_custom_parse_frame_ahx(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data) {
    /* 0xFFF5E0C0 header: frame size 0x414 (160kbps, 22050Hz) but they actually are much shorter */
    size_t current_data_size = 0;
    size_t file_size = get_streamfile_size(stream->streamfile);

    /* find actual frame size by looking for the next frame header */
    {
        uint32_t current_header = (uint32_t)read_32bitBE(stream->offset, stream->streamfile);
        off_t next_offset = 0x04;

        while (next_offset <= MPEG_AHX_EXPECTED_FRAME_SIZE) {
            uint32_t next_header = (uint32_t)read_32bitBE(stream->offset + next_offset, stream->streamfile);

            if (current_header == next_header) {
                current_data_size = next_offset;
                break;
            }

            /* AHXs end in a 0x0c footer (0x41485845 0x28632943 0x52490000 / "AHXE" "(c)C" "RI\0\0") */
            if (stream->offset + next_offset + 0x0c >= file_size) {
                current_data_size = next_offset;
                break;
            }

            next_offset++;
        }
    }
    if (!current_data_size || current_data_size > data->buffer_size || current_data_size > MPEG_AHX_EXPECTED_FRAME_SIZE) {
        VGM_LOG("MPEG AHX: incorrect data_size 0x%x\n", current_data_size);
        goto fail;
    }


    /* read VBR frames with CBR header, 0-fill up to expected size to keep mpg123 happy */
    data->bytes_in_buffer = read_streamfile(data->buffer,stream->offset,current_data_size,stream->streamfile);
    memset(data->buffer + data->bytes_in_buffer,0, MPEG_AHX_EXPECTED_FRAME_SIZE - data->bytes_in_buffer);
    data->bytes_in_buffer = MPEG_AHX_EXPECTED_FRAME_SIZE;


    /* encryption 0x08 modifies a few bits in the side_data every frame, here we decrypt the buffer */
    if (data->config.encryption) {
        VGM_LOG("MPEG AHX: unknown encryption\n");
        goto fail;
    }


    /* update offsets */
    stream->offset += current_data_size;
    if (stream->offset + 0x0c >= file_size)
        stream->offset = file_size; /* move after 0x0c footer to reach EOF (shouldn't happen normally) */


    return 1;
fail:
    return 0;
}


#endif
