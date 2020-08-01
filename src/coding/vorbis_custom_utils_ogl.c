#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * OGL removes the Ogg layer and uses 16b packet headers, that have the size of the next packet, but
 * the lower 2b need to be removed (usually 00 but 01 for the id packet, not sure about the meaning).
 */
int vorbis_custom_setup_init_ogl(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    off_t offset = start_offset;
    size_t packet_size;

    /* read 3 packets with triad (id/comment/setup), each with an OGL header */

    /* normal identificacion packet */
    packet_size = (uint16_t)read_16bitLE(offset, sf) >> 2;
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer,offset+2,packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */
    offset += 2+packet_size;

    /* normal comment packet */
    packet_size = (uint16_t)read_16bitLE(offset, sf) >> 2;
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer,offset+2,packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */
    offset += 2+packet_size;

    /* normal setup packet */
    packet_size = (uint16_t)read_16bitLE(offset, sf) >> 2;
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer,offset+2,packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */
    offset += 2+packet_size;

    /* data starts after triad */
    data->config.data_start_offset = offset;

    return 1;

fail:
    return 0;
}


int vorbis_custom_parse_packet_ogl(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    size_t bytes;

    /* get next packet size from the OGL 16b header (upper 14b) */
    data->op.bytes = (uint16_t)read_16bitLE(stream->offset, stream->streamfile) >> 2;
    stream->offset += 2;
    if (data->op.bytes == 0 || data->op.bytes == 0xFFFF || data->op.bytes > data->buffer_size) goto fail; /* EOF or end padding */

    /* read raw block */
    bytes = read_streamfile(data->buffer,stream->offset, data->op.bytes,stream->streamfile);
    stream->offset += data->op.bytes;
    if (bytes != data->op.bytes) goto fail; /* wrong packet? */

    return 1;

fail:
    return 0;
}

#endif
