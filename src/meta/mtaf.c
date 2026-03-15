#include "meta.h"
#include "../util.h"


static void get_name(VGMSTREAM* vgmstream, STREAMFILE* sf);

/* MTAF - found in Metal Gear Solid 3: Snake Eater (PS2), Subsistence and HD too */
VGMSTREAM* init_vgmstream_mtaf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;
    int32_t loop_start, loop_end;


    /* checks */
    if (!is_id32be(0x00, sf, "MTAF"))
        return NULL;
    /* .mta: actual extension in xor'ed filename
     * .mtaf: header ID */
    if (!check_extensions(sf,"mta,mtaf"))
        return NULL;
    // 0x04: rough file size (close but sometimes smaller?)
    // 0x08: version? (0, compared to MTA2 = 1)
    // 0x0c-0x20: null
    // 0x20-0x40: optional filename (xor ^ 0xFF)


    /* HEAD chunk */
    if (!is_id32be(0x40, sf, "HEAD"))
        return NULL;
    if (read_u32le(0x44, sf) != 0xB0)
        return NULL;
    // 0x48: null
    // 0x4c: related to channels? (16 or 2)
    // 0x50: 127 (volume?)
    // 0x54: 64 (pan?)
    // 0x56: null
    // 0x58: loop start sample
    // 0x5c: loop end
    // 0x60: full block size (0x110 * tracks)
    // 0x64: loop start frame (sample / 0x100)
    // 0x68: loop end frame (sample / 0x100)
    // 0x6c: null
    // 0x70: flags (00/05/07)
    // 0x74: channel flags?
    // 0x78: null
    // 0x7c: null
    // 0x78 .. 0xf8: null

    loop_start = read_s32le(0x58, sf);
    loop_end   = read_s32le(0x5c, sf);
    channels   = read_s32le(0x60, sf) / 0x110 * 2; // block size to tracks
    loop_flag  = read_s32le(0x70, sf) & 1;
    

    /* TRKP chunks (x16, per max channels) */
    // 0x00: -1=unused, 0x00 or others (0x20) if used
    // 0x04: 127 (volume?)
    // 0x05: null
    // 0x06: 64 (pan?)
    // 0x07: null
    // 0x2c: always 1
    // 0x30: x24 16-bit (volumes or ADPCM related?)
    // 0x60: -1 x4


    /* DATA chunk */
    if (!is_id32be(0x7f8, sf, "DATA"))
        return NULL;
    // 0x7fc: data size (without blocks in case of blocked layout)

    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 48000;
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_MTAF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x110 / 2; // kinda hacky for MTAF (stereo codec) track layout
    vgmstream->meta_type = meta_MTAF;

    get_name(vgmstream, sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* read encrypted name */
static void get_name(VGMSTREAM* vgmstream, STREAMFILE* sf) {
    uint8_t str[32+1] = {0};

    if (STREAM_NAME_SIZE < sizeof(str))
        return;

    read_streamfile(str, 0x20, 32, sf);
    if (str[0] == 0x00) //not all files have names
        return;
    for (int i = 0; i < 32; i++) {
        str[i] ^= 0xFF;
    }
    
    memcpy(vgmstream->stream_name, str, sizeof(str));
}
