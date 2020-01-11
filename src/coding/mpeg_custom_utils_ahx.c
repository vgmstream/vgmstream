#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG
#define MPEG_AHX_EXPECTED_FRAME_SIZE 0x414

static int ahx_decrypt_type08(uint8_t * buffer, mpeg_custom_config *config);

/* writes data to the buffer and moves offsets, transforming AHX frames as needed */
int mpeg_custom_parse_frame_ahx(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    size_t current_data_size = 0;
    size_t file_size = get_streamfile_size(stream->streamfile);

    /* AHX has a 0xFFF5E0C0 header with frame size 0x414 (160kbps, 22050Hz) but they actually are much shorter */

    /* read supposed frame size first (to minimize reads) */
    ms->bytes_in_buffer = read_streamfile(ms->buffer, stream->offset, MPEG_AHX_EXPECTED_FRAME_SIZE, stream->streamfile);

    /* find actual frame size by looking for the next frame header */
    {
        uint32_t current_header = get_u32be(ms->buffer);
        int next_pos = 0x04;

        while (next_pos <= MPEG_AHX_EXPECTED_FRAME_SIZE) {
            uint32_t next_header = get_u32be(ms->buffer + next_pos);

            if (current_header == next_header) {
                current_data_size = next_pos;
                break;
            }

            /* AHXs end in a 0x0c footer (0x41485845 28632943 52490000 / "AHXE(c)CRI\0\0") */
            if (stream->offset + next_pos + 0x0c >= file_size) {
                current_data_size = next_pos;
                break;
            }

            next_pos++;
        }
    }

    if (current_data_size == 0 || current_data_size > ms->buffer_size || current_data_size > MPEG_AHX_EXPECTED_FRAME_SIZE) {
        VGM_LOG("MPEG AHX: incorrect data_size 0x%x\n", current_data_size);
        goto fail;
    }

    /* 0-fill up to expected size to keep mpg123 happy */
    memset(ms->buffer + current_data_size, 0, MPEG_AHX_EXPECTED_FRAME_SIZE - current_data_size);
    ms->bytes_in_buffer = MPEG_AHX_EXPECTED_FRAME_SIZE;


    /* decrypt if needed */
    switch(data->config.encryption) {
        case 0x00: break;
        case 0x08: ahx_decrypt_type08(ms->buffer, &data->config); break;
        default:
            VGM_LOG("MPEG AHX: unknown encryption 0x%x\n", data->config.encryption);
            break; /* garbled frame */
    }

    /* update offsets */
    stream->offset += current_data_size;
    if (stream->offset + 0x0c >= file_size)
        stream->offset = file_size; /* skip 0x0c footer to reach EOF (shouldn't happen normally) */

    return 1;
fail:
    return 0;
}

/* Decrypts an AHX type 0x08 (keystring) encrypted frame. Algorithm by Thealexbarney */
static int ahx_decrypt_type08(uint8_t * buffer, mpeg_custom_config *config) {
    int i, index, encrypted_bits;
    uint32_t value;
    uint16_t current_key;

    /* encryption 0x08 modifies a few bits every frame, here we decrypt and write to data buffer */

    /* derive keystring to 3 primes, using the type 0x08 method, and assign each an index of 1/2/3 (0=no key) */
    /* (externally done for now, see: https://github.com/Thealexbarney/VGAudio/blob/2.0/src/VGAudio/Codecs/CriAdx/CriAdxKey.cs) */

    /* read 2b from a bitstream offset to decrypt, and use it as an index to get the key.
     * AHX encrypted bitstream starts at 107b (0x0d*8+3), every frame, and seem to always use index 2 */
    value = get_u32be(buffer + 0x0d);
    index = (value >> (32-3-2)) & 0x03;
    switch(index) {
        case 0: current_key = 0; break;
        case 1: current_key = config->cri_key1; break;
        case 2: current_key = config->cri_key2; break;
        case 3: current_key = config->cri_key3; break;
        default: goto fail;
    }

    /* AHX for DC: 16b, normal: 6b (no idea, probably some Layer II field) */
    encrypted_bits = config->cri_type == 0x10 ? 16 : 6;

    /* decrypt next bitstream 2b pairs, up to 16b (max key size):
     * - read 2b from bitstream (from higher to lower)
     * - read 2b from key (from lower to higher)
     * - XOR them to decrypt */
    for (i = 0; i < encrypted_bits; i+=2) {
        uint32_t xor_2b = (current_key >> i) & 0x03;
        value ^= ((xor_2b << (32-3-2-2)) >> i);
    }

    /* write output */
    put_32bitBE(buffer + 0x0d, value);

    return 1;
fail:
    return 0;
}

#endif
