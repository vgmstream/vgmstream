#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * AWC removes the Ogg layer and uses 32b frame sizes for headers and 16b frame sizes for data
 */
int vorbis_custom_setup_init_awc(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    off_t offset = data->config.header_offset;
    size_t packet_size;

    /* read 3 packets with triad (id/comment/setup), each with an AWC header */

    /* normal identificacion packet */
    packet_size = read_u32le(offset, sf);
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer, offset + 0x04, packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0)
        goto fail;
    offset += 0x04 + packet_size;

    /* normal comment packet */
    packet_size = read_u32le(offset, sf);
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer, offset + 0x04, packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0)
        goto fail;
    offset += 0x04 + packet_size;

    /* normal setup packet */
    packet_size = read_u32le(offset, sf);
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer, offset + 0x04, packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0)
        goto fail;
    offset += 0x04 + packet_size;

    /* data starts separate */
    data->config.data_start_offset = start_offset;

    return 1;

fail:
    return 0;
}

/* as AWC was coded by wackos, frames are forced to fit 0x800 chunks and rest is padded, in both sfx and music/blocked modes
 * (ex. read frame  until 0x7A0 + next frame is size 0x140 > pads 0x60 and last goes to next chunk) */
static inline off_t find_padding_awc(off_t offset, vorbis_custom_codec_data* data) {
    offset = offset - data->config.data_start_offset;
    
    return 0x800 - (offset % 0x800);
}

int vorbis_custom_parse_packet_awc(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    size_t bytes;

    /* get next packet size from the AWC 16b header, except between chunks */
    data->op.bytes = read_u16le(stream->offset, stream->streamfile);
    if (data->op.bytes == 0) { // || (stream->offset - start & 0x800) < 0x01 //todo could pad near block end?
        stream->offset += find_padding_awc(stream->offset, data);
        data->op.bytes = read_u16le(stream->offset, stream->streamfile);
    }

    stream->offset += 0x02;
    if (data->op.bytes == 0 || data->op.bytes == 0xFFFF || data->op.bytes > data->buffer_size)
        goto fail; /* EOF or end padding */

    /* read raw block */
    bytes = read_streamfile(data->buffer, stream->offset, data->op.bytes, stream->streamfile);
    stream->offset += data->op.bytes;
    if (bytes != data->op.bytes) goto fail; /* wrong packet? */

    return 1;

fail:
    return 0;
}

#endif
