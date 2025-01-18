#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"


 /* PPHD - Sony PSP bank format [Parappa the Rapper (PSP), Tales of Phantasia (PSP)] */
VGMSTREAM* init_vgmstream_pphd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "PPHD"))
        return NULL;
    // 0x04: chunk size
    // 0x08: version? 0x00010000 LE
    // 0x0c: -1

    if (!check_extensions(sf, "phd"))
        return NULL;

    // bank format mainly for sequences, similar to .HD/HD3
    // sections: PPPG (sequences?) > PPTN (notes?) > PPVA (streams)
    // 0x10: PPPG offset
    // 0x14: PPTN offset
    uint32_t ppva_offset = read_u32le(0x18, sf);
    // rest: reserved (-1 xN)

    meta_header_t h = {
        .meta = meta_PPHD,
    };

    h.target_subsong = sf->stream_index;
    if (h.target_subsong == 0)
        h.target_subsong = 1;

    if (!is_id32be(ppva_offset + 0x00,sf, "PPVA"))
        return NULL;
    // 04: ppva size
    // 08: info start?
    // 0c: -1
    // 10: null
    h.total_subsongs = read_s32le(ppva_offset + 0x14,sf);
    // 18: -1
    // 1c: -1

    uint32_t info_offset = ppva_offset + 0x20 + 0x10 * (h.target_subsong - 1);
    // often there is an extra subsong, a quirk shared with .hb/hd3 (may be 0-padding instead)
    uint32_t max_offset = ppva_offset + 0x20 + 0x10 * h.total_subsongs;
    if (max_offset < get_streamfile_size(sf) && read_s32le(max_offset, sf) > 0)
        h.total_subsongs += 1;


    // header
    h.stream_offset = read_u32le(info_offset + 0x00, sf);
    h.sample_rate   = read_s32le(info_offset + 0x04,sf);
    h.stream_size   = read_u32le(info_offset + 0x08,sf);
    uint32_t flags  = read_u32le(info_offset + 0x0c,sf);

    if (flags != 0xFFFFFFFF) {
        vgm_logi("PPHD: unknown header flags (report)\n");
        return NULL;
    }

    h.channels = 1;
    //h.loop_flag = (flags & 1); //TODO test of loops is always full
    h.num_samples = ps_bytes_to_samples(h.stream_size, h.channels);
    h.loop_start = 0;
    h.loop_end = h.num_samples;

    h.coding = coding_PSX;
    h.layout = layout_none;
    h.open_stream = true;
    h.has_subsongs = true;

    h.sf_head = sf;
    h.sf_body = open_streamfile_by_ext(sf,"pbd");
    if (!h.sf_body) goto fail;

    vgmstream = alloc_metastream(&h);
    close_streamfile(h.sf_body);
    return vgmstream;
fail:
    close_streamfile(h.sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
