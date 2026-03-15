#include "meta.h"
#include "mta2_streamfile.h"


/* MTA2 - found in Metal Gear Solid 4 (PS3) */
VGMSTREAM* init_vgmstream_mta2(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    int32_t loop_start, loop_end;


    /* checks */
    if (!is_id32be(0x00, sf, "MTA2"))
        return NULL;
    if (!check_extensions(sf,"mta2"))
        return NULL;
    // 0x04: file size
    // 0x08: version? (1)
    // 0x0c-0x20: null
    // 0x20-0x40: null (no name unlike .mta?)

    /* everything is very similar to MGS3's MTAF but BE */

    if (!is_id32be(0x40, sf, "HEAD"))
        return NULL;
    if (read_u32be(0x44, sf) != 0xB0)
        return NULL;
    // 0x48: null
    // 0x4c: related to channels? (16 or 2)
    // 0x50: 127 (volume?)
    // 0x54: 64 (pan?)
    // 0x56: channels (counting all tracks)
    // 0x58: loop start sample
    // 0x5c: loop end
    // 0x60: full block size (0x110 * channels)
    // 0x64: loop start frame?
    // 0x68: loop end frame?
    // 0x6c: null
    // 0x70: flags (00/05/07)
    // 0x7c: null?
    // 0x78: null
    // 0x7c: sample rate in 32b float (WHY?) typically 48000.0
    /* 0x80 .. 0xf8: null */

    channels   = read_u16be(0x56, sf);
    loop_start = read_s32be(0x58, sf);
    loop_end   = read_s32be(0x5c, sf);
    loop_flag = (loop_start != loop_end); // also flags like .mta?

    sample_rate = (int)read_f32be(0x7c, sf);
    if (sample_rate == 0)
        sample_rate = 48000; /* default when not specified (most of the time) */


    /* TRKP chunks (x16, per max channels) */
    // 0x00: -1=unused, 0x00 or others (0x20) if used
    // 0x04: 127 (volume?)
    // 0x05: sometimes 127 (volume?)
    // 0x06: 64 (pan?)
    // 0x07: channel layout (ex. 1ch = 0x04, 3ch = 0x07, 4ch = 0x33, 6ch = 0x3f)
    // 0x2c: always 1
    // 0x30: x24 16-bit (volumes or ADPCM related?)
    // 0x60: -1 x4


    /* DATA chunk */
    if (!is_id32be(0x7f8, sf, "DATA"))
        return NULL;
    // 0x7fc: data size (without blocks in case of blocked layout)

    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_MTA2;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_MTA2;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

/* MTA2 in containers */
VGMSTREAM* init_vgmstream_mta2_container(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;


    /* checks */
    /* .dbm: iPod metadata + mta2 with KCEJ blocks, .bgm: mta2 with KCEJ blocks (fake?) */
    if (!check_extensions(sf,"dbm,bgm,mta2"))
        return NULL;

    if (is_id32be(0x00,sf, "DLBM")) {
        subfile_offset = 0x800;
    }
    else if (read_32bitBE(0x00,sf) == 0x00000010) {
        subfile_offset = 0x00;
    }
    else {
        return NULL;
    }
    /* subfile size is implicit in KCEJ blocks */

    temp_sf = setup_mta2_streamfile(sf, subfile_offset, 1, "mta2");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_mta2(temp_sf);
    close_streamfile(temp_sf);

    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
