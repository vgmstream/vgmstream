#ifndef _OOR_HELPERS_H
#define _OOR_HELPERS_H
#include <stdint.h>
#include <stdbool.h>
#include "../../util/bitstream_msb.h"

/**
 * OOR has custom pages similar to OggS but not quite equivalent (needs to fiddle with packets).
 * Header and setup packets are custom, but audio packets are standard.
 * Format is bitpacked and some parts may be ambiguous, but it should fail midway on bad data.
 * Lib expects the whole file in memory and isn't well tuned for streaming though.
 *
 * Info from:
 * - mostly https://github.com/tsudoko/deoptimizeobs (FORMAT doc describes .oor, but some info is slightly off)
 * - some bits from decompilation
 */

#define OOR_FLAG_CONTINUED      (1<<3)  // 1000 = continued (first packet in page is part of last; equivalent to OggS 0x01)
#define OOR_FLAG_PARTIAL        (1<<2)  // 0100 = partial (last packet in page is not complete and must be merged with prev; OggS uses size 0xFF for this)
#define OOR_FLAG_BOS            (1<<1)  // 0010 = beginning-of-stream (irst page, OggS 0x02)
#define OOR_FLAG_EOS            (1<<0)  // 0001 = end-of-stream (last page, OggS 0x04)
//#define OOR_FLAG_NORMAL       0       // any other regular packet


typedef struct {
    uint8_t version;
    uint8_t flags;
    int64_t granule;
    int padding1;
    int padding2;
} oor_page_t;

typedef struct {
    uint8_t vps_bits;
    uint8_t padding1;
    uint8_t packet_count;
    uint8_t bps_selector;
    int16_t base_packet_size; // max 7FF (may be 0)
    int16_t variable_packet_size[256]; // max 7FFF of N packet_count (may be 0)
    uint8_t post_padding;
} oor_size_t;

typedef struct {
    uint8_t pre_padding;
    uint8_t version;
    uint8_t channels;
    int sample_rate;
    uint8_t unknown1;
    uint8_t unknown2;
    uint8_t unknown3;
    int64_t last_granule;
    uint8_t blocksize0_exp;
    uint8_t blocksize1_exp;
    uint8_t framing;
    uint8_t post_padding;
} oor_header_t;

// codebooks are set in plugin with a table of size + codebook
typedef struct {
    int type;
    int codebook_id;
    //int codebook_size;
    //uint8_t* codebook;
} oor_setup_t;

// page headers, assumes buf in bitstream (probable max ~0x200)
void oor_read_page(bitstream_t* is, oor_page_t* page);
void oor_read_size(bitstream_t* is, oor_size_t* size);
void oor_read_header(bitstream_t* is, oor_header_t* hdr);
void oor_read_setup(bitstream_t* is, oor_setup_t* setup);

bool oor_validate_header_page(oor_page_t* page, oor_header_t* hdr);
bool oor_validate_setup_page(oor_page_t* page, oor_size_t* size, oor_header_t* hdr);
bool oor_validate_setup_info(oor_page_t* page, oor_size_t* size, oor_setup_t* setup);
bool oor_validate_audio_page(oor_page_t* page, oor_size_t* size, oor_header_t* hdr);

#endif
