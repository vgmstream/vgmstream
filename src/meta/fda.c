#include "meta.h"
#include "../coding/coding.h"

/* FDA - from Relic Entertainment games [Warhammer 4000: Dawn of War (PC)] */
VGMSTREAM* init_vgmstream_fda(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, bitrate, sample_rate, num_samples;


    /* checks */
    if (!check_extensions(sf, "fda"))
        goto fail;

    if (!is_id64be(0x00, sf, "Relic Ch") ||
        !is_id64be(0x08, sf, "unky\r\n\x1a\x00"))
        goto fail;

    /* version? (later .fda change this) */
    if (read_u32le(0x10, sf) != 1 ||
        read_u32le(0x14, sf) != 1)
        goto fail;

    /* read chunks (abridged) */
    {
        off_t offset = 0x18;
        size_t chunk_size, name_size, data_size;

        /* Relic Chunkys have: type, subtype, version, data size, name size, then
         * name (optional) and data. But format has fixed chunks so we'll simplify a bit. */

        /* DATA-FBIF (file info) */
        chunk_size = read_u32le(offset + 0x0c, sf);
        name_size  = read_u32le(offset + 0x10, sf);
        offset += 0x14 + name_size + chunk_size;

        /* FOLD-FDA (folder of chunks) */
        if (!is_id32be(offset + 0x04, sf, "FDA "))
            goto fail;
        offset += 0x14;

        /* DATA-INFO (header) */
        if (!is_id32be(offset + 0x04, sf, "INFO"))
            goto fail;
        chunk_size = read_u32le(offset + 0x0c, sf);
        name_size  = read_u32le(offset + 0x10, sf);
        offset += 0x14 + name_size;

        channels   = read_s32le(offset + 0x00, sf);
        /* 0x04: bps */
        bitrate         = read_s32le(offset + 0x08, sf);
        sample_rate     = read_s32le(offset + 0x0c, sf);
        /* 0x10: loop start? (always 0) */
        /* 0x14: loop end? (always -1) */
        /* 0x18: loop offset? (always 0) */
        loop_flag = 0; /* never set? */
        offset += chunk_size;

        /* DATA-DATA (data) */
        if (!is_id32be(offset + 0x04, sf, "DATA"))
            goto fail;
        chunk_size = read_u32le(offset + 0x0c, sf);
        name_size  = read_u32le(offset + 0x10, sf);
        offset += 0x14 + name_size;

        data_size = read_u32le(offset + 0x00, sf);

        start_offset = offset + 0x04;
        num_samples = relic_bytes_to_samples(data_size, channels, bitrate);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FDA;
    vgmstream->sample_rate = 44100; /* fixed output */
    vgmstream->num_samples = num_samples;

    vgmstream->codec_data = init_relic(channels, bitrate, sample_rate);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_RELIC;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
