#include "meta.h"
#include "../coding/coding.h"


/* .ATSL - Koei Tecmo audio container [One Piece Pirate Warriors (PS3), Warriors All-Stars (PC)] */
VGMSTREAM* init_vgmstream_atsl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int total_subsongs, target_subsong = sf->stream_index;
    int big_endian = 0, entries, version, platform, format;
    uint32_t subfile_offset = 0, subfile_size = 0, header_size, entry_size;

    init_vgmstream_t init_vgmstream = NULL;
    const char* fake_ext = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "ATSL"))
        goto fail;

    /* .atsl: common extension (PC/PS4/etc)
     * .atsl3: PS3 games
     * .atsl4: some PS4 games
     * .atslx: X360 games */
    if (!check_extensions(sf,"atsl,atsl3,atsl4,atslx"))
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

    /* 0x08: unknown, varies */

    /* 0x0c(1): 0/1 (version? seen both in even in the same game) */
    version = read_u8(0x0c, sf); /* <~2017 = v0, both seen in Nioh (PC) */
    platform = read_u8(0x0d, sf);
    /* 0x0e: header version? (00~04: size 0x28, 05~06: size 0x30), 03~05 seen in Nioh (PS4) */
    /* 0x0f: always 1? */

    big_endian = (platform == 0x02 || platform == 0x03); /* PS3/X360 */

    /* Example flags at 0x08/0x0c for reference:
     * - 00010101 00020001  .atsl3 from One Piece Pirate Warriors (PS3)[ATRAC3]
     * - 00000201 00020001  .atsl3 from Fist of North Star: Ken's Rage 2 (PS3)[ATRAC3]-bgm007
     * - 01010301 00030201  .atsl3 from Fist of North Star: Ken's Rage 2 (X360)[ATRAC3]-bgm007
     *   00000301 00020101  (same)
     * - 01040301 00060301  .atsl4 from Nobunaga's Ambition: Sphere of Influence (PS4)[ATRAC9]
     * - 00060301 00040301  .atsl in G1L from One Piece Pirate Warriors 3 (Vita)[ATRAC9]
     * - 00060301 00010301  .atsl in G1L from One Piece Pirate Warriors 3 (PC)[KOVS]
     * - 000A0301 00010501  .atsl in G1L from Warriors All-Stars (PC)[KOVS] 2017-09
     * - 000B0301 00080601  .atsl in G1l from Sengoku Musou Sanada Maru (Switch)[KTSS] 2017-09
     * - 01000000 00010501  .atsl from Nioh (PC)[KOVS] 2017-11
     * - 01000000 01010501  .atsl from Nioh (PC)[KOVS] 2017-11
     * - 03070301 01060301  .atsl from Nioh (PS4)[ATRAC9]
     * - 00080301 01060401  .atsl from Nioh (PS4)[ATRAC9]
     * - 00090301 01060501  .atsl from Nioh (PS4)[ATRAC9] (bigger header)
     * - 010C0301 01060601  .atsl from Dynasty Warriors 9 (PS4)[KTAC]
     * - 010D0301 01010601  .atsl from Dynasty Warriors 9 DLC (PC)[KOVS]
     */

    switch(version) {
        case 0x00:
            entry_size = 0x28;
            break;
        case 0x01:
            entry_size = 0x3c;
            break;
        default:
            vgm_logi("ATSL: unknown version %x (report)\n", version);
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

            /* entry header (values match subfile header) */
            /* 0x00: id (0..N, usually) */
            entry_subfile_offset = read_u32(header_size + i*entry_size + 0x04,sf);
            entry_subfile_size  = read_u32(header_size + i*entry_size + 0x08,sf);
            if (version == 0x00) {
                format = 0x00;
                /* 0x0c: sample rate */
                /* 0x10: samples */
                /* 0x14: loop start */
                /* 0x18: loop end */
                /* 0x1c: null? */
                /* 0x28: offset to unknown extra table (after entries) */
                /* 0x0c: unknown table entries (0x2C each) */
            }
            else {
                /* 0x0c: null? */
                /* 0x10: offset to unknown extra table (after entries) */
                /* 0x14: unknown table entries (0x2C each) */
                /* 0x18: channels */
                /* 0x1c: some hash/id? (same for all entries) */
                format = read_u32(header_size + i*entry_size + 0x20, sf);
                /* 0x24: sample rate */
                /* 0x28: samples */
                /* 0x2c: encoder delay */
                /* 0x30: loop start (includes delay) */
                /* 0x34: channel layout */
                /* 0x38: null? */
            }

            //TODO use silence subsong?
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

        if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;
        if (!subfile_offset || !subfile_size) goto fail;

        /* some kind of switch table may follow (referenced in entries) */
    }

    /* similar codec values also seen in KTSR */
    switch(platform) {
        case 0x01: /* PC */
            if (format == 0x0000 || format == 0x0005) {
                init_vgmstream = init_vgmstream_ogg_vorbis;
                fake_ext = "kvs";
            }
            break;

        case 0x02: /* PS3 */
            if (format == 0x0000) {
                init_vgmstream = init_vgmstream_riff;
                fake_ext = "at3";
            }
            break;

        case 0x03: /* X360 */
            if (format == 0x0000) {
                init_vgmstream = init_vgmstream_xma;
                fake_ext = "xma";
            }
            break;

        case 0x04: /* Vita */
        case 0x06: /* PS4 */
            if (format == 0x0000 || format == 0x1001) { /* Nioh (PS4)-1001 */
                init_vgmstream = init_vgmstream_riff;
                fake_ext = "at9";
            }
            else if (format == 0x1000) { /* Dynasty Warriors 9 (PS4) patch BGM */
                init_vgmstream = init_vgmstream_ktac;
                fake_ext = "ktac";
            }
            break;

        case 0x08: /* Switch */
            if (format == 0x0000 || format == 0x0005) {/* Dynasty Warriors 9 (Switch)-0005 w/ Opus */
                init_vgmstream = init_vgmstream_ktss;
                fake_ext = "ktss";
            }
            break;

        default:
            break;
    }

    if (init_vgmstream == NULL  || fake_ext == NULL) {
        vgm_logi("ATSL: unknown platform %x + format %x (report)\n", platform, format);
        goto fail;
    }


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
