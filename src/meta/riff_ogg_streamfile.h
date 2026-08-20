#ifndef _RIFF_OGG_STREAMFILE_H_
#define _RIFF_OGG_STREAMFILE_H_
#include "deblock_streamfile.h"

typedef struct {
    uint32_t patch_offset;
} riff_ogg_io_data;

static size_t riff_ogg_io_read(STREAMFILE* sf, uint8_t* dst, uint32_t offset, size_t length, riff_ogg_io_data* data) {
    size_t bytes = read_streamfile(dst, offset, length, sf);

    /* has garbage init Oggs pages, patch bad flag */
    if (data->patch_offset && data->patch_offset >= offset && data->patch_offset < offset + bytes) {
        VGM_ASSERT(dst[data->patch_offset - offset] != 0x02, "RIFF Ogg: bad patch offset at %x\n", data->patch_offset);
        dst[data->patch_offset - offset] = 0x00;
    }

    return bytes;
}

static size_t ogg_get_page(uint8_t* buf, size_t bufsize, uint32_t offset, STREAMFILE* sf) {
    size_t segments, bytes, page_size;

    if (0x1b > bufsize)
        return 0;
    bytes = read_streamfile(buf, offset, 0x1b, sf);
    if (bytes != 0x1b)
        return 0;

    segments = get_u8(buf + 0x1a);
    if (0x1b + segments > bufsize)
        return 0;

    bytes = read_streamfile(buf + 0x1b, offset + 0x1b, segments, sf);
    if (bytes != segments)
        return 0;

    page_size = 0x1b + segments;
    for (int i = 0; i < segments; i++) {
        page_size += get_u8(buf + 0x1b + i);
    }

    return page_size;
}

/* Patches "pseudo-CBR" Ogg from vorbis.acm's RIFFs.
 * In "+" modes the ACM encoder adds an extra stream (ID = -1) with padding pages. However, the 2nd header
 * page is empty/invalid and causes glitches in decoders (probably triggers re-sync + consumes bytes from 1st).
 * Other pages from the 2nd stream seem ignored, except the last few near EOF, that also confuse decoders.
 * This streamfile patches out the page header (patch_offset), and detects the first stream's EOF (real_size).
 *
 * Could remove unwanted pages on-the-fly, but it's a bit problematic due to arbitrary seek offsets (probably
 * should be handled in ogg_vorbis_decoder).
 */
static STREAMFILE* setup_riff_ogg_streamfile(STREAMFILE* sf, uint32_t start, size_t size) {
    uint8_t buf[0x1000];
    uint32_t patch_offset = 0;
    size_t real_size = size;

    /* find offset were the 2nd fake stream starts */
    {
        uint32_t offset = start;
        uint32_t offset_limit = start + size; // usually in the first 0x3000 but can be +0x100000
        size_t page_size;

        /* first page is ok */
        page_size = ogg_get_page(buf, sizeof(buf), offset, sf); //temp_sf);
        if (page_size == 0)
            return NULL;
        offset += page_size;

        while (offset < offset_limit) {
            page_size = ogg_get_page(buf, sizeof(buf), offset, sf); //temp_sf);
            if (page_size <= 0x04)
                break;

            if (get_u32be(buf + 0x00) != get_id32be("OggS"))
                break;

            if (get_u16be(buf + 0x04) == 0x0002) { // start page flag
                patch_offset = (offset - start) + 0x04 + 0x01; // clamp'ed
                break;
            }

            offset += page_size;
        }

        // not found: ignored during reads
        //if (patch_offset == 0)
        //    return NULL;
    }

    /* has a bunch of padding pages at the end with no data nor flag that confuse decoders, find actual end */
    {
        size_t chunk_size = sizeof(buf); /* not worth testing more */
        size_t max_size = size;
        size_t pos;
        uint32_t read_offset = start + size - chunk_size;

        pos = read_streamfile(buf, read_offset, chunk_size, sf);
        if (read_offset < 0 || pos <= 0x1a) return NULL;

        pos -= 0x1a; /* at least one OggS page */
        while (pos > 0) {
            if (get_u32be(buf + pos + 0x00) == get_id32be("OggS")) {

                if (get_u16be(buf + pos + 0x04) == 0x0004) { /* last page flag is ok */
                    real_size = max_size;
                    break;
                }
                else { /* last page flag is wrong */
                    max_size = size - (chunk_size - pos); /* update size up to this page */
                }
            }
            pos--;
        }
    }

    /* actual custom streamfile init */
    {
        STREAMFILE* new_sf = NULL;
        riff_ogg_io_data io_data = {0};

        io_data.patch_offset = patch_offset;

        new_sf = open_wrap_streamfile(sf);
        new_sf = open_clamp_streamfile_f(new_sf, start, real_size);
        new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(riff_ogg_io_data), riff_ogg_io_read, NULL);
        return new_sf;
    }
}

#endif
