#include "meta.h"
#include "../layout/layout.h"
#include "../util/endianness.h"


/* .AST - from Nintendo games [Super Mario Galaxy (Wii/Switch), Pac-Man Vs (GC)] */
VGMSTREAM* init_vgmstream_ast(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;
    bool big_endian;

    /* checks */
    if (is_id32be(0x00, sf, "STRM")) {
        big_endian = true;
    }
    else if (is_id32be(0x00, sf, "MRTS")) {
        big_endian = false; // Super Mario 3D All-Stars - Super Mario Galaxy (Switch), Pikmin 2 (Switch)
    }
    else {
        return NULL;
    }

    if (!check_extensions(sf, "ast"))
        return NULL;

    read_u32_t read_u32 = big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = big_endian ? read_s32be : read_s32le;
    read_u16_t read_u16 = big_endian ? read_u16be : read_u16le;
   
    if (read_u32(0x04,sf) + 0x40 != get_streamfile_size(sf))
        return NULL;
    int codec   = read_u16be(0x08,sf); // always big-endian?
    channels    = read_u16(0x0c,sf);
    if (read_u16(0x0a,sf) != 16) // spf?
        return NULL;
    loop_flag   = read_u16(0x0e,sf);

    //20: max_block?
    if (read_u32(0x40, sf) != get_id32be("BLCK"))
        return NULL;
    start_offset  = 0x40;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AST;
    vgmstream->sample_rate = read_s32(0x10,sf);
    vgmstream->num_samples = read_s32(0x14,sf);
    vgmstream->loop_start_sample = read_s32(0x18,sf);
    vgmstream->loop_end_sample = read_s32(0x1c,sf);
    vgmstream->codec_endian = big_endian;

    vgmstream->layout_type = layout_blocked_ast;
    switch (codec) {
        case 0x00: // Pikmin 2 (GC/Switch)
            vgmstream->coding_type = coding_AFC;
            break;
        case 0x01: /* Mario Kart: Double Dash!! (GC) */
            vgmstream->coding_type = coding_PCM16BE;
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
