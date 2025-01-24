#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"


 /* HD+BD / HBD - Sony PS2 bank format [Parappa the Rapper 2 (PS2), Vib-Ripple (PS2)] */
VGMSTREAM* init_vgmstream_hd_bd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    if (!is_id64be(0x00,sf, "IECSsreV"))
        return NULL;
    // 0x08: full section size
    // 0x0c: version? 0x01010000, 0x00020000 LE

    // .hd: standard
    // .hdb: found in PrincessSoft's PS2 games (.PAC bigfiles don't seem to have names but exe does refers to HDB)
    if (!check_extensions(sf, "hd,hbd"))
        return NULL;

    // bank format mainly for sequences, sometimes used for sfx/voices or pseudo-streams with bgm.
    // sections: Vers > Head > Vagi (streams) > Smpl (notes?) > Sset > Prog (sequences?)
    uint32_t head_offset = 0x10;
    if (!is_id64be(head_offset + 0x00,sf, "IECSdaeH"))
        return NULL;
    uint32_t hd_size = read_u32le(head_offset + 0x0c, sf);
    uint32_t bd_size = read_u32le(head_offset + 0x10, sf);
    // 0x14: Prog offset
    // 0x18: Sset offset
    // 0x1c: Smpl offset
    uint32_t vagi_offset = read_u32le(head_offset + 0x20, sf);
    // 0x24: Setb offset
    // rest: reserved (-1, or rarely 0 [Midnight Club 2 (PS2)])

    meta_header_t h = {
        .meta = meta_HD_BD,
    };

    h.target_subsong = sf->stream_index;
    if (h.target_subsong == 0)
        h.target_subsong = 1;

    if (!is_id64be(vagi_offset + 0x00,sf, "IECSigaV"))
        return NULL;
    //vagi_size = read_u32le(vagi_offset + 0x08,sf); // including id/size
    h.total_subsongs = read_s32le(vagi_offset + 0x0c,sf);

    // mini offset table, though all vagi headers seem pasted together
    uint32_t info_offset = read_u32le(vagi_offset + 0x10 + 0x04 * (h.target_subsong - 1), sf) + vagi_offset;
    // often there is an extra subsong, a quirk shared with .hb2/phd (except in PrincessSoft's games?)
    // (last subsongs doesn't seem to be related to others or anything like that)
    // after all table entries there is always 32b padding so it should be fine to check as null
    uint32_t null_offset = read_u32le(vagi_offset + 0x10 + 0x04 * h.total_subsongs, sf);
    if (null_offset != 0)
        h.total_subsongs += 1;
    // in PrincessSoft's games last subsongs seems dummy and sets offset to internal .bd end
    uint32_t last_offset = read_u32le(vagi_offset + 0x10 + 0x04 * (h.total_subsongs - 1), sf);
    if (last_offset && read_u32le(vagi_offset + last_offset + 0x00, sf) == bd_size)
        h.total_subsongs -= 1;

    // vagi header
    h.stream_offset = read_u32le(info_offset + 0x00, sf);
    h.sample_rate   = read_u16le(info_offset + 0x04,sf);
    uint8_t flags   = read_u8   (info_offset + 0x06,sf);
    uint8_t unknown = read_u8   (info_offset + 0x07,sf); // 0x00 in v1.1, 0xFF in v2.0 (?)

    if (flags > 0x01 || (unknown != 0x00 && unknown != 0xFF)) {
        vgm_logi("HD+BD: unknown header flags (report)\n");
        return NULL;
    }

    // calc size via next offset
    uint32_t next_offset;
    if (h.target_subsong == h.total_subsongs) {
        next_offset = bd_size;
    }
    else {
        uint32_t nextinfo_offset = read_u32le(vagi_offset + 0x10 + 0x04 * (h.target_subsong - 1 + 1), sf) + vagi_offset;
        next_offset = read_u32le(nextinfo_offset + 0x00, sf);
    }
    h.stream_size = next_offset - h.stream_offset;

    h.channels = 1;
    h.loop_flag = (flags & 1); //TODO test of loops is always full
    h.num_samples = ps_bytes_to_samples(h.stream_size, h.channels);
    h.loop_start = 0;
    h.loop_end = h.num_samples;

    h.coding = coding_PSX;
    h.layout = layout_none;
    h.open_stream = true;
    h.has_subsongs = true;

    // detect hdb pasted together (handle as a separate meta?)
    if (get_streamfile_size(sf) == hd_size + bd_size) {
        if (!check_extensions(sf, "hbd"))
            goto fail;

        h.sf_head = sf;
        h.sf_body = sf;
        h.stream_offset += hd_size;
    }
    else {
        if (get_streamfile_size(sf) != hd_size)
            goto fail;

        h.sf_head = sf;
        h.sf_body = open_streamfile_by_ext(sf,"bd");
        if (!h.sf_body) goto fail;

        if (get_streamfile_size(h.sf_body) != bd_size)
            goto fail;
    }

    vgmstream = alloc_metastream(&h);
    if (sf != h.sf_body) close_streamfile(h.sf_body);
    return vgmstream;
fail:
    if (sf != h.sf_body) close_streamfile(h.sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
