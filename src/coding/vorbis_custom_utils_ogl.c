#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * OGL uses 16b packet headers (14b size + 2b flags, usually 00 but 01 for the id packet),
 * with standard header packet triad.
 */
int vorbis_custom_setup_init_ogl(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    uint32_t offset = start_offset;
    uint32_t packet_size;

    packet_size = read_u16le(offset, sf) >> 2;
    if (!load_header_packet(sf, data, packet_size, 0x02, &offset)) /* identificacion packet */
        goto fail;

    packet_size = read_u16le(offset, sf) >> 2;
    if (!load_header_packet(sf, data, packet_size, 0x02, &offset)) /* comment packet */
        goto fail;

    packet_size = read_u16le(offset, sf) >> 2;
    if (!load_header_packet(sf, data, packet_size, 0x02, &offset)) /* setup packet */
        goto fail;

    /* data starts after triad */
    data->config.data_start_offset = offset;

    return 1;
fail:
    return 0;
}


int vorbis_custom_parse_packet_ogl(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    size_t bytes;

    /* get next packet size */
    data->op.bytes = read_u16le(stream->offset, stream->streamfile) >> 2;
    stream->offset += 2;
    if (data->op.bytes == 0 || data->op.bytes == 0xFFFF || data->op.bytes > data->buffer_size) goto fail; /* EOF or end padding */

    /* read raw block */
    bytes = read_streamfile(data->buffer, stream->offset, data->op.bytes, stream->streamfile);
    stream->offset += data->op.bytes;
    if (bytes != data->op.bytes) goto fail; /* wrong packet? */

    return 1;
fail:
    return 0;
}

#endif
