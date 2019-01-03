#include "meta.h"
#include "../coding/coding.h"

typedef enum { ATRAC3, ATRAC9, KOVS, KTSS, KTAC } atsl_codec;

/* .ATSL - Koei Tecmo audio container [One Piece Pirate Warriors (PS3), Warriors All-Stars (PC)] */
VGMSTREAM * init_vgmstream_atsl(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    int total_subsongs, target_subsong = streamFile->stream_index;
    int type, big_endian = 0, entries;
    atsl_codec codec;
    const char* fake_ext;
    off_t subfile_offset = 0;
    size_t subfile_size = 0, header_size, entry_size;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .atsl: header id (for G1L extractions), .atsl3: PS3 games, .atsl4: PS4 games */
    if ( !check_extensions(streamFile,"atsl,atsl3,atsl4"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4154534C) /* "ATSL" */
        goto fail;

    /* main header (LE) */
    header_size = read_32bitLE(0x04,streamFile);
    /* 0x08/0c: flags?, 0x10: fixed? (0x03E8) */
    entries = read_32bitLE(0x14,streamFile);
    /* 0x18: 0x28, or 0x30 (rarer) */
    /* 0x1c: null, 0x20: subheader size, 0x24/28: null */

    /* Type byte may be wrong (could need header id tests instead). Example flags at 0x08/0x0c:
     * - 00010101 00020001  .atsl3 from One Piece Pirate Warriors (PS3)[ATRAC3]
     * - 00000201 00020001  .atsl3 from Fist of North Star: Ken's Rage 2 (PS3)[ATRAC3]
     *   00000301 00020101  (same)
     * - 01040301 00060301  .atsl4 from Nobunaga's Ambition: Sphere of Influence (PS4)[ATRAC9]
     * - 00060301 00040301  atsl in G1L from One Piece Pirate Warriors 3 (Vita)[ATRAC9]
     * - 00060301 00010301  atsl in G1L from One Piece Pirate Warriors 3 (PC)[KOVS]
     * - 000A0301 00010501  atsl in G1L from Warriors All-Stars (PC)[KOVS]
     * - 000B0301 00080601  atsl in G1l from Sengoku Musou Sanada Maru (Switch)[KTSS]
     * - 010C0301 01060601  .atsl from Dynasty Warriors 9 (PS4)[KTAC]
     */
    entry_size = 0x28;
    type = read_16bitLE(0x0c, streamFile);
    switch(type) {
        case 0x0100:
            codec = KOVS;
            fake_ext = "kvs";
            break;
        case 0x0200:
            codec = ATRAC3;
            fake_ext = "at3";
            big_endian = 1;
            break;
        case 0x0400:
        case 0x0600:
            codec = ATRAC9;
            fake_ext = "at9";
            break;
        case 0x0601:
            codec = KTAC;
            fake_ext = "ktac";
            entry_size = 0x3c;
            break;
        case 0x0800:
            codec = KTSS;
            fake_ext = "ktss";
            break;
        default:
            VGM_LOG("ATSL: unknown type %x\n", type);
            goto fail;
    }
    read_32bit = big_endian ? read_32bitBE : read_32bitLE;


    /* entries can point to the same file, count unique only */
    {
        int i,j;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        /* parse entry header (in machine endianness) */
        for (i = 0; i < entries; i++) {
            int is_unique = 1;

            /* 0x00: id */
            off_t entry_subfile_offset = read_32bit(header_size + i*entry_size + 0x04,streamFile);
            size_t entry_subfile_size  = read_32bit(header_size + i*entry_size + 0x08,streamFile);
            /* 0x08+: channels/sample rate/num_samples/loop_start/etc (match subfile header) */

            /* check if current entry was repeated in a prev entry */
            for (j = 0; j < i; j++)  {
                off_t prev_offset = read_32bit(header_size + j*entry_size + 0x04,streamFile);
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


    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, fake_ext);
    if (!temp_streamFile) goto fail;

    /* init the VGMSTREAM */
    switch(codec) {
        case ATRAC3:
        case ATRAC9:
            vgmstream = init_vgmstream_riff(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
#ifdef VGM_USE_VORBIS
        case KOVS:
            vgmstream = init_vgmstream_ogg_vorbis(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
#endif
        case KTSS:
            vgmstream = init_vgmstream_ktss(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case KTAC:
            //vgmstream = init_vgmstream_ktac(temp_streamFile); //Koei Tecto VBR-like ATRAC9
            //if (!vgmstream) goto fail;
            //break;
        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
