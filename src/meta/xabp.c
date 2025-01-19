#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"


 /* XABp - cavia PS2 bank format [Drakengard 1/2 (PS2), Ghost in the Shell: SAC (PS2)] */
VGMSTREAM* init_vgmstream_xabp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "pBAX"))
        return NULL;

    // .hd2+bd: from bigfiles
    if (!check_extensions(sf, "hd2"))
        return NULL;

    meta_header_t h = {
        .meta = meta_XABP,
    };
    h.target_subsong = sf->stream_index;
    if (h.target_subsong == 0)
        h.target_subsong = 1;

    // cavia's bank format inspired by .hd+bd
    uint32_t bd_size = read_u32le(0x04,sf);
    // 0x08: null
    h.total_subsongs = read_s16le(0x0c,sf);
    // 0x0e: always 0x0010?

    uint32_t head_offset = 0x10 + 0x20 * (h.target_subsong - 1);
    // 00: file id?
    h.sample_rate   = read_s16le(head_offset + 0x16,sf);
    h.stream_offset = read_u32le(head_offset + 0x18, sf);
    // others: config? (can't make sense of them, don't seem quite like sizes/flags/etc)

    h.channels = 1;

    h.coding = coding_PSX;
    h.layout = layout_none;
    h.open_stream = true;
    h.has_subsongs = true;

    h.sf_head = sf;
    h.sf_body = open_streamfile_by_ext(sf,"bd");
    if (!h.sf_body) goto fail;

    // Entries/offsets aren't ordered .bd not it seems to have sizes (maybe mixes notes+streams into one)
    // Since PS-ADPCM is wired to play until end frame end or loop, it's probably designed like that.
    // It also repeats entries (different ID but same config) but for now just prints it as is; this also happens in bigfiles.
    h.loop_flag = ps_find_stream_info(h.sf_body, h.stream_offset, bd_size - h.stream_offset, h.channels, h.interleave, &h.loop_start, &h.loop_end, &h.stream_size);
    h.num_samples = ps_bytes_to_samples(h.stream_size, h.channels);

    vgmstream = alloc_metastream(&h);
    close_streamfile(h.sf_body);
    return vgmstream;
fail:
    close_streamfile(h.sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
