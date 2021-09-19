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
    int i;

    if (0x1b > bufsize) goto fail;
    bytes = read_streamfile(buf, offset, 0x1b, sf);
    if (bytes != 0x1b) goto fail;

    segments = get_u8(buf + 0x1a);
    if (0x1b + segments > bufsize) goto fail;

    bytes = read_streamfile(buf + 0x1b, offset + 0x1b, segments, sf);
    if (bytes != segments) goto fail;

    page_size = 0x1b + segments;
    for (i = 0; i < segments; i++) {
        page_size += get_u8(buf + 0x1b + i);
    }

    return page_size;
fail:
    return 0;
}

/* patches Ogg with weirdness */
static STREAMFILE* setup_riff_ogg_streamfile(STREAMFILE* sf, uint32_t start, size_t size) {
    uint32_t patch_offset = 0;
    size_t real_size = size;
    uint8_t buf[0x1000];


    /* initial page flag is repeated and causes glitches in decoders, find bad offset */
    //todo callback could patch on-the-fly by analyzing all "OggS", but is problematic due to arbitrary offsets
    {
        uint32_t offset = start;
        size_t page_size;
        uint32_t offset_limit = start + size; /* usually in the first 0x3000 but can be +0x100000 */
        //todo this doesn't seem to help much
        STREAMFILE* temp_sf = reopen_streamfile(sf, 0x100); /* use small-ish sf to avoid reading the whole thing */

        /* first page is ok */
        page_size = ogg_get_page(buf, sizeof(buf), offset, temp_sf);
        offset += page_size;

        while (offset < offset_limit) {
            page_size = ogg_get_page(buf, sizeof(buf), offset, temp_sf);
            if (page_size == 0) break;

            if (get_u32be(buf + 0x00) != get_id32be("OggS"))
                break;

            if (get_u16be(buf + 0x04) == 0x0002) { /* start page flag */
                //;VGM_ASSERT(patch_offset > 0, "RIFF Ogg: found multiple repeated start pages\n");
                patch_offset = (offset - start) + 0x04 + 0x01; /* clamp'ed */
                break;
            }

            offset += page_size;
        }

        close_streamfile(temp_sf);

        /* no need to patch initial flag */
        //if (patch_offset == 0)
        //    return NULL;
    }

    /* has a bunch of padding(?) pages at the end with no data nor flag that confuse decoders, find actual end */
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

#endif /* _RIFF_OGG_STREAMFILE_H_ */
