#include "meta.h"
#include "../coding/coding.h"


/* .ATSL - Koei Tecmo audio container [One Piece Pirate Warriors (PS3), Warriors All-Stars (PC)] */
VGMSTREAM* init_vgmstream_atsl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int total_subsongs, target_subsong = sf->stream_index;
    int type, big_endian = 0, entries;
    uint32_t subfile_offset = 0, subfile_size = 0, header_size, entry_size;

    init_vgmstream_t init_vgmstream = NULL;
    const char* fake_ext;


    /* checks */
    if (!is_id32be(0x00,sf, "ATSL"))
        goto fail;
    /* .atsl: header id (for G1L extractions), .atsl3: PS3 games, .atsl4: PS4 games */
    if (!check_extensions(sf,"atsl,atsl3,atsl4"))
        goto fail;

    /* main header (LE) */
    header_size = read_u32le(0x04,sf);
    /* 0x08/0c: flags? */
    /* 0x10: volume? (always 1000) */
    entries = read_u32le(0x14,sf);
    /* 0x18: 0x28, or 0x30 (rarer) */
    /* 0x1c: null */
    /* 0x20: subheader size */
    /* 0x24/28: null */

    /* Type byte may be wrong (could need header id tests instead). Example flags at 0x08/0x0c:
     * - 00010101 00020001  .atsl3 from One Piece Pirate Warriors (PS3)[ATRAC3]
     * - 00000201 00020001  .atsl3 from Fist of North Star: Ken's Rage 2 (PS3)[ATRAC3]
     *   00000301 00020101  (same)
     * - 01040301 00060301  .atsl4 from Nobunaga's Ambition: Sphere of Influence (PS4)[ATRAC9]
     * - 00060301 00040301  .atsl in G1L from One Piece Pirate Warriors 3 (Vita)[ATRAC9]
     * - 00060301 00010301  .atsl in G1L from One Piece Pirate Warriors 3 (PC)[KOVS]
     * - 000A0301 00010501  .atsl in G1L from Warriors All-Stars (PC)[KOVS] 2017-09
     * - 01000000 01010501  .atsl from Nioh (PC)[KOVS] 2017-11
     * - 01000000 00010501  .atsl from Nioh (PC)[KOVS] 2017-11
     * - 000B0301 00080601  .atsl in G1l from Sengoku Musou Sanada Maru (Switch)[KTSS] 2017-09
     * - 010C0301 01060601  .atsl from Dynasty Warriors 9 (PS4)[KTAC]
     * - 010D0301 01010601  .atsl from Dynasty Warriors 9 DLC (PC)[KOVS]
     */

    type = read_u16le(0x0c, sf);
    //version = read_u16le(0x0e, sf);
    switch(type) {
        case 0x0100: /* KOVS */
            init_vgmstream = init_vgmstream_ogg_vorbis;
            fake_ext = "kvs";
            entry_size = 0x28;
            break;
        case 0x0101:
            init_vgmstream = init_vgmstream_ogg_vorbis;
            fake_ext = "kvs";
            entry_size = 0x3c;
            break;
        case 0x0200: /* ATRAC3 */
            init_vgmstream = init_vgmstream_riff;
            fake_ext = "at3";
            entry_size = 0x28;
            big_endian = 1;
            break;
        case 0x0400:
        case 0x0600: /* ATRAC9 */
            init_vgmstream = init_vgmstream_riff;
            fake_ext = "at9";
            entry_size = 0x28;
            break;
        case 0x0601: /* KTAC */
            init_vgmstream = init_vgmstream_ktac;
            fake_ext = "ktac";
            entry_size = 0x3c;
            break;
        case 0x0800: /* KTSS */
            init_vgmstream = init_vgmstream_ktss;
            fake_ext = "ktss";
            entry_size = 0x28;
            break;
        default:
            vgm_logi("ATSL: unknown type %x (report)\n", type);
            goto fail;
    }

    /* entries can point to the same file, count unique only */
    {
        int i, j;
        uint32_t (*read_u32)(off_t,STREAMFILE*) = big_endian ? read_u32be : read_u32le;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        /* parse entry header (in machine endianness) */
        for (i = 0; i < entries; i++) {
            int is_unique = 1;
            uint32_t entry_subfile_offset, entry_subfile_size;

            /* 0x00: id */
            entry_subfile_offset = read_u32(header_size + i*entry_size + 0x04,sf);
            entry_subfile_size  = read_u32(header_size + i*entry_size + 0x08,sf);
            /* 0x10+: config, channels/sample rate/num_samples/loop_start/channel layout/etc (matches subfile header) */

            /* dummy entry, seen in DW9 DLC (has unique config though) */
            if (!entry_subfile_offset && !entry_subfile_size)
                continue;

            /* check if current entry was repeated in a prev entry */
            for (j = 0; j < i; j++)  {
                off_t prev_offset = read_u32(header_size + j*entry_size + 0x04,sf);
                if (prev_offset == entry_subfile_offset) {
                    is_unique = 0;
                    break;
                }
            }
            if (!is_unique)
                continue;

            total_subsongs++;

            /* target GET, but keep going to count subsongs */
            if (!subfile_offset && target_subsong == total_subsongs) {
                subfile_offset = entry_subfile_offset;
                subfile_size   = entry_subfile_size;
            }
        }
    }
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;
    if (!subfile_offset || !subfile_size) goto fail;

    /* some kind of seek/switch table may follow (optional, found in .atsl3) */

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, fake_ext);
    if (!temp_sf) goto fail;

    /* init the VGMSTREAM */
    vgmstream = init_vgmstream(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
