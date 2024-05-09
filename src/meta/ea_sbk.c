#include "meta.h"
#include "../util/endianness.h"


/* .SBK - EA Redwood Shores soundbank (Simpsons Game, Godfather) */
VGMSTREAM* init_vgmstream_ea_sbk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int target_stream = sf->stream_index;
    off_t chunk_offset;
    size_t chunk_size;

    /* checks */
    if (!is_id32be(0x00, sf, "sbnk") && /* sbnk */
        !is_id32le(0x00, sf, "sbnk"))   /* knbs */
        return NULL;

    if (!check_extensions(sf, "sbk"))
        return NULL;


    read_u32_t read_u32 = is_id32be(0x00, sf, "sbnk") ? read_u32le : read_u32be;

    chunk_offset = read_u32(0x10, sf);
    chunk_size = read_u32(0x14, sf);

    if (chunk_offset + chunk_size != get_streamfile_size(sf))
        goto fail;


    if (!target_stream) target_stream = 1;
    target_stream -= 1;

    if (is_id32be(chunk_offset, sf, "BNKl") ||
        is_id32be(chunk_offset, sf, "BNKb")) {
        /* The Godfather */

        vgmstream = load_vgmstream_ea_bnk(sf, chunk_offset, target_stream, 0); /* unsure about is_embedded */
        if (!vgmstream) goto fail;

        vgmstream->meta_type = meta_EA_SBK;
    }
    else if (is_id32be(chunk_offset, sf, "sdat") || /* sdat */
             is_id32le(chunk_offset, sf, "sdat")) { /* tads */
        /* The Simpsons Game */

        int total_subsongs;
        off_t entry_offset, stream_offset;

        eaac_meta_t info = {0};

        total_subsongs = read_u32(chunk_offset + 0x04, sf);
        entry_offset = chunk_offset + 0x8 + target_stream * 0x10;

        /* For each entry:
         * 0x00: stream index
         * 0x04: 0x0313BABE (?)
         * 0x08: stream offset
         * 0x0C: 0xFEEDFEED (?)
         */

        if (read_u32(entry_offset + 0x00, sf) != target_stream)
            goto fail;

        stream_offset = chunk_offset + read_u32(entry_offset + 0x08, sf);

        info.sf_head = sf;
        info.sf_body = sf;
        info.head_offset = stream_offset;
        info.type = meta_EA_SBK;

        vgmstream = load_vgmstream_ea_eaac(&info);
        if (!vgmstream) goto fail;

        vgmstream->num_streams = total_subsongs;
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
