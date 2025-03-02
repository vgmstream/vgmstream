#ifdef VGM_USE_VORBIS
#include <string.h>
#include <math.h>
#include "oor_helpers.h"


// .oor is divided into pages like "OggS", but simplified (variable sized)
// - v0: 0x01
// - v1: 0x0A
void oor_read_page(bitstream_t* is, oor_page_t* page) {
    uint32_t granule_hi = 0, granule_lo = 0; //bitreader only handles 32b

    page->version    = bm_read(is, 2);
    page->flags      = bm_read(is, 4); // vorbis packet flags

    // extra fields in V1
    switch(page->version) {
        case 0:
            page->granule  = 0;
            page->padding1 = 0;
            page->padding2 = 0;
            // bitstream ends with into bit 6 of current byte
            break;

        case 1:
            page->padding1  = bm_read(is, 2); // padding as granule must be byte-aligned
            granule_hi      = bm_read(is, 32);
            granule_lo      = bm_read(is, 32);
            page->granule   = ((uint64_t)granule_hi << 32) | granule_lo;
            page->padding2  = bm_read(is, 6); // padding to leave bitstream into bit 6, like v0
            break;
        
        default: // unknown version
            break;
    }
}

// each .oor page except the first (header) has N packets:
// size is variable but theoretical max is ~0x200 (plus page header)
void oor_read_size(bitstream_t* is, oor_size_t* size) {
    // right after page info (meaning 6 bits into bitstream)
    size->vps_bits      = bm_read(is, 4);
    size->padding1      = bm_read(is, 1);
    size->packet_count  = bm_read(is, 8);
    size->bps_selector  = bm_read(is, 2);

    switch(size->bps_selector) {
        case 0: size->base_packet_size = 0; break; // 0 bits
        case 1: size->base_packet_size = bm_read(is, 8); break;
        case 2: size->base_packet_size = bm_read(is, 11); break;
        case 3: //undefined
        default: 
            size->base_packet_size = 0;
            break;
    }

    for (int i = 0; i < size->packet_count; i++) {
        if (size->vps_bits) {
            size->variable_packet_size[i] = bm_read(is, size->vps_bits);
        }
        else {
            size->variable_packet_size[i] = 0; //reset in case of reusing struct
        }
    }

    uint32_t bit_pos = bm_pos(is) % 8;
    if (bit_pos > 0) {
        size->post_padding = bm_read(is, 8 - bit_pos); // aligned to next byte
    }
    else {
        size->post_padding = 0;
    }

    // bitstream is now byte-aligned, and next is N packets of base + variable packet sizes (in bytes)
}

// bit-packed header (variable-sized):
// - v0 (sr_selector!=3): 0x02
// - v0 (sr_selector==3): 0x03
// - v1 (sr_selector!=3): 0x12
// - v1 (sr_selector==3): 0x13
void oor_read_header(bitstream_t* is, oor_header_t* hdr) {
    hdr->pre_padding    = bm_read(is, 2); // from first page, header is byte-aligned

    hdr->version        = bm_read(is, 2);
    hdr->channels       = bm_read(is, 3);
    int sr_selector     = bm_read(is, 2);

    if (sr_selector == 3) {
        int sr_index    = bm_read(is, 8);

        // few known modes, though they have 8 bits
        // seems v1 lib can handle v0 files but sample rate is not compatible, maybe an oversight
        // (note that some games somehow have both v0 and v1 files)
        switch(hdr->version) {
            case 0:
                switch(sr_index) {
                    case 3: hdr->sample_rate = 32000; break;
                    case 4: hdr->sample_rate = 48000; break;
                    case 5: hdr->sample_rate = 96000; break;
                    default: hdr->sample_rate = 0; break;
                }

            case 1:
                switch(sr_index) {
                    case 4: hdr->sample_rate = 32000; break;
                    case 5: hdr->sample_rate = 48000; break;
                    case 6: hdr->sample_rate = 64000; break;
                    case 7: hdr->sample_rate = 88200; break;
                    case 8: hdr->sample_rate = 96000; break;
                    default: hdr->sample_rate = 0; break;
                }
                break;

            default:
                break;
        }

        // shouldn't happen, but allow to catch missing cases
        if (hdr->sample_rate == 0) // && sr_index < 256
            hdr->sample_rate = 8000;
    }
    else {
        hdr->sample_rate = 11025 * pow(2, sr_selector);
    }

    if (hdr->version == 1) {
        uint32_t granule_hi = 0, granule_lo = 0; //bitreader only handles 32b

        hdr->unknown1   = bm_read(is, 1); // always 1?
        hdr->unknown2   = bm_read(is, 1); // always 1?
        hdr->unknown3   = bm_read(is, 7); // always 0?

        granule_hi      = bm_read(is, 32);
        granule_lo      = bm_read(is, 32);
        hdr->last_granule = ((uint64_t)granule_hi << 32) | granule_lo;
    }
    else {
        hdr->unknown1   = 0;
        hdr->unknown2   = 0;
        hdr->unknown3   = 0;
        hdr->last_granule = 0;
    }

    hdr->blocksize1_exp  = bm_read(is, 4);
    hdr->blocksize0_exp  = bm_read(is, 4);
    hdr->framing         = bm_read(is, 1); //always 1

    uint32_t bit_pos = bm_pos(is) % 8;
    if (bit_pos > 0) {
        hdr->post_padding = bm_read(is, 8 - bit_pos); // aligned to next byte
    }
    else {
        hdr->post_padding = 0;
    }
}

// bit-packed setup (should be byte-aligned)
void oor_read_setup(bitstream_t* is, oor_setup_t* setup) {
    setup->type         = bm_read(is, 2);
    setup->codebook_id  = bm_read(is, 6);
    // known codebooks:
    // 1~6: common on PC games
    // 7: Muv-Luv, Muv-Luv Alternative (Vita)
}


// extra validations since bitpacket headers are a bit simple
bool oor_validate_header_page(oor_page_t* page, oor_header_t* hdr) {
    if (page->version > 1 || page->flags != 0x02)
        return false;

    if (page->granule != 0 || page->padding1 != 0 || page->padding2 != 0)
        return false;

    if (hdr->pre_padding != 0 || hdr->version > 1 || hdr->version != page->version)
        return false;
    if (hdr->channels == 0 || hdr->sample_rate == 0)
        return false;
    //if (hdr->channels > OOR_MAX_CHANNELS) // known max is 2, allow to detect other cases
    //    return false;

    if (hdr->version == 1 && hdr->last_granule == 0)
        return false;

    if (hdr->blocksize0_exp < 6 || hdr->blocksize0_exp > 13 || hdr->blocksize1_exp < 6 || hdr->blocksize1_exp > 13)
        return false;
    if (hdr->framing != 1)
        return false;

    if (hdr->post_padding != 0)
        return false;

    return true;
}

bool oor_validate_setup_page(oor_page_t* page, oor_size_t* size, oor_header_t* hdr) {
    if (page->version != hdr->version)
        return false;
    // oddly enough codebooks may be split into other pages using OOR_FLAG_PARTIAL
    if (page->flags & OOR_FLAG_CONTINUED || page->flags & OOR_FLAG_BOS || page->flags & OOR_FLAG_EOS)
        return false;

    if (page->granule != 0 || page->padding1 != 0 || page->padding2 != 0)
        return false;
    if (size->padding1 != 0 || size->bps_selector == 3)
        return false;
    if (size->post_padding != 0)
        return false;

    //if (setup->type != 0x01)
    //    return false;
    //if (setup->codebook_id > OOR_MAX_CODEBOOK_ID) // known max is 7, allow to detect other cases
    //    return false;

    return true;
}

bool oor_validate_setup_info(oor_page_t* page, oor_size_t* size, oor_setup_t* setup) {

    // setup packet always has 2 packets: setup info + vorbis codebook
    if (size->packet_count != 2)
        return false;
    int packet0_size = size->base_packet_size + size->variable_packet_size[0];
    int packet1_size = size->base_packet_size + size->variable_packet_size[1];

    if (packet0_size != 0x01)
        return false;
    // size is 0 when using codebook ids
    if ((setup->codebook_id > 0 && packet1_size != 0) || (setup->codebook_id == 0 && packet1_size == 0))
        return false;
    return true;
}

bool oor_validate_audio_page(oor_page_t* page, oor_size_t* size, oor_header_t* hdr) {
    if (hdr != NULL && page->version != hdr->version)
        return false;
    if (page->flags & OOR_FLAG_BOS)
        return false;
    if (page->padding1 != 0 || page->padding2 != 0)
        return false;

    if (size->padding1 != 0 || size->bps_selector == 3)
        return false;
    if (size->post_padding != 0)
        return false;

    if (size->packet_count == 0)
        return false;

    return true;
}

#endif
