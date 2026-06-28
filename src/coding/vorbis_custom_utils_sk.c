#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

static bool get_page_info(STREAMFILE* sf, off_t page_offset, off_t* p_packet_offset, size_t* p_packet_size, int* p_page_packets, int* p_header_type, int target_packet);
static int build_header(uint8_t* buf, size_t bufsize, STREAMFILE* sf, off_t packet_offset, size_t packet_size);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * SK just replaces the id 0x4F676753 ("OggS") by 0x11534B10 (\11"SK"\10), and the word "vorbis" by "SK"
 * in init packets (for obfuscation, surely). So essentially we are parsing regular Ogg here.
 *
 * A simpler way to implement this may be using ogg_vorbis_file with read callbacks (pretend this is proof of concept).
 */
int vorbis_custom_setup_init_sk(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    off_t offset = start_offset;
    off_t id_offset = 0, comment_offset = 0, setup_offset = 0;
    size_t id_size = 0, comment_size = 0, setup_size = 0;
    int page_packets;

    /* standard header packets except the "vorbis" keyword is replaced by "SK" */

    /* first page has the id packet */
    if (!get_page_info(sf, offset, &id_offset, &id_size, &page_packets, NULL, 0)) goto fail;
    if (page_packets != 1) goto fail;
    offset = id_offset + id_size;

    /* second page has the comment and setup packets */
    if (!get_page_info(sf, offset, &comment_offset, &comment_size, &page_packets, NULL, 0)) goto fail;
    if (page_packets != 2) goto fail;
    if (!get_page_info(sf, offset, &setup_offset, &setup_size, &page_packets, NULL, 1)) goto fail;
    if (page_packets != 2) goto fail;
    offset = comment_offset + comment_size + setup_size;


    /* init with all offsets found */
    data->op.bytes = build_header(data->buffer, data->buffer_size, sf, id_offset, id_size);
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */

    data->op.bytes = build_header(data->buffer, data->buffer_size, sf, comment_offset, comment_size);
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */

    data->op.bytes = build_header(data->buffer, data->buffer_size, sf, setup_offset, setup_size);
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */

    /* data starts after triad */
    data->config.data_start_offset = offset;

    return 1;

fail:
    return 0;
}

// TODO: improve (handle more oor like pre-loading page data into buf)
int vorbis_custom_parse_packet_sk(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    off_t packet_offset = 0;
    size_t packet_size = 0;
    int page_packets;
    bool ok;

    /* read OggS/SK page and get current packet */
    ok = get_page_info(stream->streamfile, stream->offset, &packet_offset, &packet_size, &page_packets, NULL, data->current_packet);
    data->current_packet++;
    if (!ok || packet_size > data->buffer_size) return false;

    data->op.bytes = read_streamfile(data->buffer, packet_offset, packet_size, stream->streamfile);
    if (data->op.bytes != packet_size) return false;

    /* go next page when processed all packets in page */
    if (data->current_packet >= page_packets) {
        size_t prev_size = packet_size;

        // special call to advance current page to next one
        ok = get_page_info(stream->streamfile, stream->offset, &packet_offset, &packet_size, &page_packets, NULL, -1);
        if (!ok) return false;
        stream->offset = packet_offset + packet_size;
        data->current_packet = 0;

        // 'continued packet' (first packet is part of prev packet), signaled by size 0xFF in the last packet of 
        // prev page + header_type of the next page. New continued packet can be of size 0x00 to allow packets of size 0xFF.
        if (prev_size == 0xFF) {
            int header_type = 0;

            ok = get_page_info(stream->streamfile, stream->offset, &packet_offset, &packet_size, &page_packets, &header_type, 0);
            if (!ok || prev_size + packet_size > data->buffer_size) return false;

            if (header_type & 0x01) {
                data->current_packet++;

                data->op.bytes += read_streamfile(data->buffer + prev_size, packet_offset, packet_size, stream->streamfile);
                if (data->op.bytes != prev_size + packet_size) return false;
            }
        }
    }

    return true;
}

/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

/**
 * Get packet info from an Ogg page, from segment/packet N (-1 = all segments)
 *
 * Page format:
 *  0x00(4): capture pattern ("OggS")
 *  0x01(1): stream structure version
 *  0x05(1): header type flag (0x01: continued packet, 0x02: first page, 0x04: last page)
 *  0x06(8): absolute granule position
 *  0x0e(4): stream serial number
 *  0x12(4): page sequence number
 *  0x16(4): page checksum
 *  0x1a(1): page segments (total bytes in segment table)
 *  0x1b(n): segment table (N bytes, 1 packet is sum of sizes until != 0xFF)
 *  0x--(n): data
 * Reference: https://xiph.org/ogg/doc/framing.html
 */
static bool get_page_info(STREAMFILE* sf, off_t page_offset, off_t* p_packet_offset, size_t* p_packet_size, int* p_page_packets, int* p_header_type, int target_packet) {
    off_t table_offset, current_packet_offset, target_packet_offset = 0;
    size_t total_packets_size = 0, current_packet_size = 0, target_packet_size = 0;


    if (read_u32be(page_offset+0x00, sf) != 0x11534B10) // \11"SK"\10
        return false;
    /* No point on validating other stuff, but they look legal enough (CRC too it seems) */

    uint8_t header_type = read_u8(page_offset+0x05, sf);
    uint8_t segments    = read_u8(page_offset+0x1a, sf);

    table_offset = page_offset + 0x1b;
    current_packet_offset = page_offset + 0x1b + segments; /* first packet starts after segments */

    /* process segments */
    int page_packets = 0;
    for (int i = 0; i < segments; i++) {
        uint8_t segment_size = read_u8(table_offset, sf);
        total_packets_size += segment_size;
        current_packet_size += segment_size;
        table_offset += 0x01;

        // packet complete or last packet (continued in next page)
        if (segment_size != 0xFF || i + 1 == segments) {
            page_packets++;

            if (target_packet + 1 == page_packets) {
                target_packet_offset = current_packet_offset;
                target_packet_size = current_packet_size;
            }

            /* keep reading to fill page_packets */
            current_packet_offset += current_packet_size; /* move to next packet */
            current_packet_size = 0;
        }
    }

    /* < 0 is accepted and returns first offset and all packets sizes */
    if (target_packet + 1 > page_packets)
        return false;
    if (target_packet < 0) {
        target_packet_offset = page_offset + 0x1b + segments; /* first */
        target_packet_size = total_packets_size;
    }

    if (p_packet_offset) *p_packet_offset = target_packet_offset;
    if (p_packet_size) *p_packet_size = target_packet_size;
    if (p_page_packets) *p_page_packets = page_packets;
    if (p_header_type) *p_header_type = header_type;

    return true;
}

/* rebuild a custom header packet into a "vorbis" one */
static int build_header(uint8_t* buf, size_t bufsize, STREAMFILE* sf, off_t packet_offset, size_t packet_size) {
    int bytes;
    int vorbis_word_size = 0x02; /* "SK" */

    if (0x07 + packet_size - 0x01 - vorbis_word_size > bufsize)
        return 0;

    put_u8(buf+0x00, read_u8(packet_offset,sf)); /* packet_type */
    memcpy(buf+0x01, "vorbis", 6); /* id */
    bytes = read_streamfile(buf+0x07, packet_offset + 0x01 + vorbis_word_size, packet_size - 0x01 - vorbis_word_size, sf); /* copy rest (all except id+"(vorbis word)") */
    if (packet_size - 0x03 != bytes)
        return 0;

    return 0x07 + packet_size - 0x03;
}

#endif
