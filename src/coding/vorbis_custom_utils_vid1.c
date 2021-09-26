#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#define BITSTREAM_READ_ONLY /* config */
#include "vorbis_bitreader.h"

#include <vorbis/codec.h>


/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

static int get_packet_header(STREAMFILE* sf, off_t* offset, size_t* size);
static int build_header_comment(uint8_t* buf, size_t bufsize);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * VID1 removes the Ogg layer and uses a block layout with custom packet headers.
 *
 * Info from hcs's vid1_2ogg: https://github.com/hcs64/vgm_ripping/tree/master/demux/vid1_2ogg
 */
int vorbis_custom_setup_init_vid1(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    off_t offset = start_offset;
    size_t packet_size = 0;

    /* read header packets (id/setup), each with an VID1 header */

    /* normal identificacion packet */
    get_packet_header(sf, &offset, &packet_size);
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer,offset,packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */
    offset += packet_size;

    /* generate comment packet */
    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */

    /* normal setup packet */
    get_packet_header(sf, &offset, &packet_size);
    if (packet_size > data->buffer_size) goto fail;
    data->op.bytes = read_streamfile(data->buffer,offset,packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */
    offset += packet_size;

    return 1;

fail:
    return 0;
}


int vorbis_custom_parse_packet_vid1(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    size_t bytes;


    /* test block start */
    if (read_32bitBE(stream->offset + 0x00,stream->streamfile) == 0x4652414D) { /* "FRAM" */
        stream->offset += 0x20;

        if (read_32bitBE(stream->offset + 0x00,stream->streamfile) == 0x56494444) { /* "VIDD"*/
            stream->offset += read_32bitBE(stream->offset + 0x04, stream->streamfile);
        }

        if (read_32bitBE(stream->offset + 0x00,stream->streamfile) == 0x41554444) { /* "AUDD" */
            data->block_offset = stream->offset;
            data->block_size   = read_32bitBE(stream->offset + 0x0c,stream->streamfile);
            stream->offset += 0x14; /* actual start, rest is chunk sizes and maybe granule info */
        }
    }


    /* get packet info the VID1 header */
    get_packet_header(stream->streamfile, &stream->offset, (size_t*)&data->op.bytes);
    if (data->op.bytes == 0 || data->op.bytes > data->buffer_size) goto fail; /* EOF or end padding */

    /* read raw block */
    bytes = read_streamfile(data->buffer,stream->offset, data->op.bytes,stream->streamfile);
    stream->offset += data->op.bytes;
    if (bytes != data->op.bytes) goto fail; /* wrong packet? */

    //todo: sometimes there are short packets like 01be590000 and Vorbis complains and skips, no idea

    /* test block end (weird size calc but seems ok) */
    if ((stream->offset - (data->block_offset + 0x14)) >= (data->block_size - 0x06)) {
        stream->offset = data->block_offset + read_32bitBE(data->block_offset + 0x04,stream->streamfile);
    }

    return 1;

fail:
    return 0;
}


/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

static int build_header_comment(uint8_t* buf, size_t bufsize) {
    int bytes = 0x19;

    if (bytes > bufsize) return 0;

    put_8bit   (buf+0x00, 0x03);            /* packet_type (comments) */
    memcpy     (buf+0x01, "vorbis", 6);     /* id */
    put_32bitLE(buf+0x07, 0x09);            /* vendor_length */
    memcpy     (buf+0x0b, "vgmstream", 9);  /* vendor_string */
    put_32bitLE(buf+0x14, 0x00);            /* user_comment_list_length */
    put_8bit   (buf+0x18, 0x01);            /* framing_flag (fixed) */

    return bytes;
}

/* read header in Vorbis bitpacking format  */
static int get_packet_header(STREAMFILE* sf, off_t* offset, size_t* size) {
    uint8_t ibuf[0x04]; /* header buffer */
    size_t ibufsize = 0x04; /* header ~max */
    bitstream_t ib = {0};
    uint32_t size_bits;


    if (read_streamfile(ibuf,(*offset),ibufsize, sf) != ibufsize)
        goto fail;

    init_bitstream(&ib, ibuf, ibufsize);

    /* read using Vorbis weird LSF */
    rv_bits(&ib,  4,&size_bits);
    rv_bits(&ib,  (size_bits+1),(uint32_t*)size);

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
