#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>
#include "libs/oor_helpers.h"

// if enabled vgmstream weights ~20kb more
#ifndef VGM_DISABLE_CODEBOOKS
#include "libs/vorbis_codebooks_oor.h"
#endif



// read current page's info and save it to persist between decode calls
static int read_page_info(vorbis_custom_codec_data* data, STREAMFILE* sf, uint32_t page_offset) {

    // bitreader data
    uint8_t buf[0x200]; // variable but shouldn't need to be bigger than this
    int buf_size = sizeof(buf);

    //TODO: this will overread a bit; it's possible to load beginning + calc size + load rest, but to simplify...
    int bytes = read_streamfile(buf, page_offset, buf_size, sf);
    if (bytes != buf_size) return false;

    bitstream_t is_tmp;
    bitstream_t* is = &is_tmp;
    bm_setup(is, buf, bytes);

    // oor bitstream info
    oor_page_t page = {0};
    oor_size_t size = {0};

    oor_read_page(is, &page);
    oor_read_size(is, &size);
    if (!oor_validate_audio_page(&page, &size, NULL))
        return 0;

    if (size.packet_count >= MAX_PACKET_SIZES) {
        VGM_LOG("OOR: packet count %i bigger than observed max (report)\n", size.packet_count);
        return 0;
    }

    //TODO: maybe put into a separate struct
    data->flags = page.flags;
    data->packet_count = size.packet_count;
    for (int i = 0; i < size.packet_count; i++) {
        data->packet_size[i] = size.base_packet_size + size.variable_packet_size[i];
    }

    int page_size = bm_pos(is) / 8; // aligned
    return page_size;
}

// Reads a single packet, which may be split into multiple pages (much like OggS). 
// returns current packet size but also advances offsets, given it may need to read multiple pages.
static int read_packet(uint8_t* buf, int buf_size, vorbis_custom_codec_data* data, STREAMFILE* sf, uint32_t* p_offset) {
    int read_size = 0;

    // handle split packets: (similar to vorbis?)
    // - OOR_FLAG_CONTINUED and is last packet of a page: packet is joined with next packet
    // - OOR_FLAG_PARTIAL and is first packet of a page: packet is joined with prev packet
    // - otherwise: read normally
    // in OOR, packets may be split like full-size (curr page) + 0-size (next page), or splitsetup packet

    while (true) {

        // OOR allows 0-sized packets marked as partial, but not sure if prev packet can be 0
        //if ((data->flags & OOR_FLAG_CONTINUED) && read_size == 0)
        //    return 0;

        // new page
        if (data->current_packet == 0) {

            // no new pages after flag is set
            if (data->flags & OOR_FLAG_EOS)
                return 0;

            int page_size = read_page_info(data, sf, *p_offset);
            if (!page_size) return 0;

            *p_offset += page_size;

            if (data->packet_count == 0) {
                VGM_LOG("OOR: empty page found\n");
                return 0;
            }
        }

        // read chunk to buf (note that size 0 is valid in rare cases)
        int packet_size = data->packet_size[data->current_packet];
        data->current_packet++;

        if (data->current_packet == data->packet_count)
            data->current_packet = 0;

        if (read_size + packet_size > buf_size) {
            VGM_LOG("OOR: packet too big\n");
            return 0;
        }

        int bytes = read_streamfile(buf + read_size, *p_offset, packet_size, sf);
        if (bytes != packet_size)
            return 0;

        *p_offset += packet_size;
        read_size += packet_size;

        // packet with continued flag must be merged with next packet (typically in next page)
        bool is_last = data->current_packet == 0; // was reset above
        if (!(is_last && (data->flags & OOR_FLAG_PARTIAL)))
            break;
    }

    return read_size;
}

// read header info (1st page) and extract info to init Vorbis
static int read_header_packet(vorbis_custom_codec_data* data, STREAMFILE* sf, uint32_t* p_offset) {

    // bitreader data
    uint8_t* buf = data->buffer;
    int hdr_size = 0x20; // variable but should be smaller
    if (hdr_size > data->buffer_size)
        return 0;

    int bytes = read_streamfile(buf, *p_offset, hdr_size, sf);
    if (bytes != hdr_size) return 0;

    bitstream_t is = {0};
    bm_setup(&is, buf, bytes);

    // oor bitstream info
    oor_page_t page = {0};
    oor_header_t hdr = {0};

    // header page
    oor_read_page(&is, &page);
    oor_read_header(&is, &hdr);
    if (!oor_validate_header_page(&page, &hdr))
        return 0;

    // load info for header (also used for output)
    vorbis_custom_config* cfg = &data->config;
    cfg->channels        = hdr.channels;
    cfg->sample_rate     = hdr.sample_rate;
    cfg->blocksize_0_exp = hdr.blocksize0_exp;
    cfg->blocksize_1_exp = hdr.blocksize1_exp;
    cfg->last_granule    = hdr.last_granule;

    uint32_t packet_size = (bm_pos(&is) / 8);
    *p_offset += packet_size;
    return packet_size;
}

// read setup info (2nd page) and load codebooks info buf
static int build_header_setup(uint8_t* buf, int buf_size, vorbis_custom_codec_data* data, STREAMFILE* sf, uint32_t* p_offset) {

    // setup info packet
    int info_size = read_packet(buf, buf_size, data, sf, p_offset);
    if (!info_size) return 0;

    if (info_size != 0x01) //fixed mini-packet
        return 0;
    bitstream_t is = {0};
    bm_setup(&is, data->buffer, info_size);

    oor_setup_t setup = {0};
    oor_read_setup(&is, &setup);

   
    // paste missing info
    if (buf_size <= 0x07)
        return 0;
    put_u8   (buf+0x00, 0x05);              // packet_type (setup)
    memcpy   (buf+0x01, "vorbis", 6);       // id

    // read actual codebook based on prev mini-packet
    int setup_size;
    if (setup.codebook_id) {
#ifndef VGM_DISABLE_CODEBOOKS
        // load setup from data in executables
        setup_size = vcb_load_codebook_array(buf + 0x07, buf_size - 0x07, setup.codebook_id, vcb_list, vcb_list_count);
        if (!setup_size) return 0;
#else
        setup_size = 0;
#endif
        // next packet is always 0 when codebook id is set
        int empty_size = read_packet(buf + 0x07 + setup_size, buf_size - 0x07 - setup_size, data, sf, p_offset);
        if (empty_size != 0) return 0;
    }
    else {
        // load setup from data in file
        setup_size = read_packet(buf + 0x07, buf_size - 0x07, data, sf, p_offset);
        if (!setup_size) return 0;
    }

    return 0x07 + setup_size;
}


int vorbis_custom_setup_init_oor(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    int ret;
    uint32_t offset = start_offset;

    // header
    int head_size = read_header_packet(data, sf, &offset);
    if (!head_size) return false;

    // identification packet
    data->op.bytes = build_header_identification(data->buffer, data->buffer_size, &data->config);
    ret = vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op);
    if (ret != 0) return false;

    // comment packet
    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    ret = vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op);
    if (ret != 0) return false;

    // setup packet
    data->op.bytes = build_header_setup(data->buffer, data->buffer_size, data, sf, &offset);
    ret = vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op);
    if (ret != 0) return false;

    data->config.data_start_offset = offset;
    return true;
}

int vorbis_custom_parse_packet_oor(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    uint32_t offset = stream->offset;

    data->op.bytes = read_packet(data->buffer, data->buffer_size, data, stream->streamfile, &offset);
    stream->offset = offset;

    return data->op.bytes != 0;
}

#endif
