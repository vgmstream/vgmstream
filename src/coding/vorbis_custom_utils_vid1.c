#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include "../util/bitstream_lsb.h"

#include <vorbis/codec.h>


/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

static int get_packet_header(STREAMFILE* sf, uint32_t* offset, uint32_t* size);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * VID1 removes the Ogg layer and uses a block layout with custom packet headers.
 * Has standard id/setup packets but removes comment packet.
 *
 * Info from hcs's vid1_2ogg: https://github.com/hcs64/vgm_ripping/tree/master/demux/vid1_2ogg
 */
int vorbis_custom_setup_init_vid1(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    uint32_t offset = start_offset;
    uint32_t packet_size = 0;

    get_packet_header(sf, &offset, &packet_size);
    if (!load_header_packet(sf, data, packet_size, 0x00, &offset)) /* identificacion packet */
        goto fail;

    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0)/* comment packet */
        goto fail; 

    get_packet_header(sf, &offset, &packet_size);
    if (!load_header_packet(sf, data, packet_size, 0x00, &offset)) /* setup packet */
        goto fail;

    return 1;
fail:
    return 0;
}


int vorbis_custom_parse_packet_vid1(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    size_t bytes;


    /* test block start */
    if (is_id32be(stream->offset + 0x00,stream->streamfile, "FRAM")) {
        stream->offset += 0x20;

        if (is_id32be(stream->offset + 0x00,stream->streamfile, "VIDD")) {
            stream->offset += read_u32be(stream->offset + 0x04, stream->streamfile);
        }

        if (is_id32be(stream->offset + 0x00,stream->streamfile, "AUDD")) {
            data->block_offset = stream->offset;
            data->block_size   = read_u32be(stream->offset + 0x0c,stream->streamfile);
            stream->offset += 0x14; /* actual start, rest is chunk sizes and maybe granule info */
        }
    }


    /* get packet info the VID1 header */
    get_packet_header(stream->streamfile, (uint32_t*)&stream->offset, (uint32_t*)&data->op.bytes);
    if (data->op.bytes == 0 || data->op.bytes > data->buffer_size) goto fail; /* EOF or end padding */

    /* read raw block */
    bytes = read_streamfile(data->buffer,stream->offset, data->op.bytes,stream->streamfile);
    stream->offset += data->op.bytes;
    if (bytes != data->op.bytes) goto fail; /* wrong packet? */

    //todo: sometimes there are short packets like 01be590000 and Vorbis complains and skips, no idea

    /* test block end (weird size calc but seems ok) */
    if ((stream->offset - (data->block_offset + 0x14)) >= (data->block_size - 0x06)) {
        stream->offset = data->block_offset + read_u32be(data->block_offset + 0x04,stream->streamfile);
    }

    return 1;

fail:
    return 0;
}


/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

/* read header in Vorbis bitpacking format  */
static int get_packet_header(STREAMFILE* sf, uint32_t* offset, uint32_t* size) {
    uint8_t ibuf[0x04]; /* header buffer */
    size_t ibufsize = 0x04; /* header ~max */
    bitstream_t ib = {0};
    uint32_t size_bits;


    if (read_streamfile(ibuf,(*offset),ibufsize, sf) != ibufsize)
        goto fail;

    bl_setup(&ib, ibuf, ibufsize);

    /* read using Vorbis weird LSF */
    bl_get(&ib,  4,&size_bits);
    bl_get(&ib,  (size_bits+1),(uint32_t*)size);

    /* special meaning, seen in silent frames */
    if (size_bits == 0 && *size == 0 && (uint8_t)read_8bit(*offset, sf) == 0x80) {
        *size = 0x01;
    }

    /* pad and convert to byte offset */
    if (ib.b_off % 8)
        ib.b_off += 8 - (ib.b_off % 8);
    *offset += (ib.b_off/8);

    return 1;
fail:
    return 0;
}

#endif
