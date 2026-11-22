#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/meta_utils.h"

/* SYGN - Bandai Namco's SOL-AVES engine Sygnus sound lib (evolution of NuSound) */
VGMSTREAM* init_vgmstream_sygn(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "SYGN"))
        return NULL;
    if (!check_extensions(sf,"soundstream"))
        return NULL;

    // 0x04: null
    if (!is_id32be(0x08, sf, "STRM"))
        return NULL;
    // 0x0c: null
    // 0x10: null
    // 0x14: data size

    // apparently other chunks are possible in non-streamed SYGN, see https://github.com/Nenkai/010GameTemplates
    if (!is_id32be(0x18, sf, "STRM"))
        return NULL;
    // 0x1c: chunk size
    // 0x20: 64-bit hash?
    uint32_t stream_size = read_u32le(0x28, sf);
    uint32_t stream_offset = 0x2c;

    // soundstreams seem to have most info in external index/bank files, so heuristics are needed.
    // Loop info also seems external, though RIFF can contain "smpl".
    if (is_id32be(0x2c, sf, "RIFF")) {
        temp_sf = setup_subfile_streamfile(sf, stream_offset, stream_size, "wav");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_riff(temp_sf);
    }
    else if (read_u32le(0x2c, sf) == 0x80000001) {
        temp_sf = setup_subfile_streamfile(sf, stream_offset, stream_size, "opus");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_opus_std(temp_sf);
    }
    else if (read_u32le(0x2c, sf) < 64) {
        meta_header_t hdr = {0};
        hdr.channels = read_s32le(0x2c, sf);
        hdr.sample_rate = read_s32le(0x30, sf);
        hdr.num_samples = read_s32le(0x34, sf);
        //block_samples = read_u32le(0x38, sf);
        hdr.stream_offset = 0x3c;
        hdr.stream_size = stream_size - 0x10;

        hdr.meta = meta_SYGN;
        hdr.coding = coding_SYGNUS;
        hdr.layout = layout_interleave;
        hdr.interleave = 0x108;
        hdr.interleave_last = get_interleave_last(hdr.stream_size, hdr.interleave, hdr.channels);
        // tiny interleave_last of 0x04 is possible, where only 1 header sample is written (aad6b9f5-adb9-4ce4-9505-438f4ce92d63)

        hdr.sf = sf;
        hdr.open_stream = true;

        vgmstream = alloc_metastream(&hdr);
    }
    else {
        goto fail;
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
