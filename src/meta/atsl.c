#include "meta.h"
#include "../coding/coding.h"

static STREAMFILE* setup_atsl_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, const char* fake_ext);
typedef enum { ATRAC3, ATRAC9, KOVS } atsl_codec;

/* .ATSL - Koei Tecmo audio container [One Piece Pirate Warriors (PS3), Warriors All-Stars (PC)] */
VGMSTREAM * init_vgmstream_atsl(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    int total_subsongs, target_subsong = streamFile->stream_index;
    int type, big_endian = 0;
    atsl_codec codec;
    const char* fake_ext;
    off_t subfile_offset;
    size_t subfile_size, header_size;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* check extensions
     * .atsl: header id (for G1L extractions), .atsl3: PS3 games, .atsl4: PS4 games */
    if ( !check_extensions(streamFile,"atsl,atsl3,atsl4"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4154534C) /* "ATSL" */
        goto fail;

    /* main header (LE) */
    header_size = read_32bitLE(0x04,streamFile);
    /* 0x08/0c: flags?, 0x10: fixed? (0x03E8) */
    total_subsongs = read_32bitLE(0x14,streamFile);
    /* 0x18: 0x28, or 0x30 (rarer) */
    /* 0x1c: null, 0x20: subheader size, 0x24/28: null */

    //todo: sometimes entries are repeated/dummy and point to the first entry
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;


    /* Type byte may be wrong (could need header id tests instead).
     * Example flags at 0x08/0x0c:
     * - 00010101 00020001  .atsl3 from One Piece Pirate Warriors (PS3)[ATRAC3]
     * - 00000201 00020001  .atsl3 from Fist of North Star: Ken's Rage 2 (PS3)[ATRAC3]
     *   00000301 00020101  (same)
     * - 01040301 00060301  .atsl4 from Nobunaga's Ambition: Sphere of Influence (PS4)[ATRAC9]
     * - 00060301 00040301  atsl in G1L from One Piece Pirate Warriors 3 (Vita)[ATRAC9]
     * - 00060301 00010301  atsl in G1L from One Piece Pirate Warriors 3 (PC)[KOVS]
     * - 000A0301 00010501  atsl in G1L from Warriors All-Stars (PC)[KOVS]
     */
    type = read_8bit(0x0d, streamFile);
    switch(type) {
        case 0x01:
            codec = KOVS;
            fake_ext = "kvs";
            break;
        case 0x02:
            codec = ATRAC3;
            fake_ext = "at3";
            big_endian = 1;
            break;
        case 0x04:
        case 0x06:
            codec = ATRAC9;
            fake_ext = "at9";
            break;
        default:
            goto fail;
    }
    read_32bit = big_endian ? read_32bitBE : read_32bitLE;


    /* entry header (machine endianness) */
    /* 0x00: id */
    subfile_offset = read_32bit(header_size + (target_subsong-1)*0x28 + 0x04,streamFile);
    subfile_size   = read_32bit(header_size + (target_subsong-1)*0x28 + 0x08,streamFile);
    /* 0x08+: sample rate/num_samples/loop_start/etc, matching subfile header */
    /* some kind of seek/switch table follows (optional, found in .atsl3) */

    temp_streamFile = setup_atsl_streamfile(streamFile, subfile_offset,subfile_size, fake_ext);
    if (!temp_streamFile) goto fail;

    /* init the VGMSTREAM */
    switch(codec) {
        case ATRAC3:
        case ATRAC9:
            vgmstream = init_vgmstream_riff(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case KOVS:
            vgmstream = init_vgmstream_ogg_vorbis(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
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


static STREAMFILE* setup_atsl_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, const char* fake_ext) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,fake_ext);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
