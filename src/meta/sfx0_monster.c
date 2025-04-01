#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


/* SFX0 - from Monster Games [NASCAR Heat 2002 (Xbox), NASCAR: Dirt to Daytona (PS2/GC), Excite Truck (Wii), ExciteBots (Wii)] */
VGMSTREAM* init_vgmstream_sfx0_monster(STREAMFILE* sf) {

    /* checks*/
    uint32_t data_size = read_u32le(0x00,sf);
    uint32_t head_size = read_u32le(0x04,sf);
    if (!data_size || !head_size || data_size + head_size != get_streamfile_size(sf))
        return NULL;
    // .sfx: common
    // .sf0: tiny .sn0 (preload?)
    if (!check_extensions(sf, "sfx,sf0"))
        return NULL;

    // SFX0 is the internal fourCC used for .sfx

    meta_header_t h = {0};
    h.meta = meta_SFX0_MONSTER;

    h.loop_flag     = read_u8   (0x08, sf);
    int extra_flag =  read_u8   (0x09, sf); // always 1 in DSP
    // 0a: null?
    int codec       = read_u16le(0x0c, sf);
    h.channels      = read_u16le(0x0e, sf);
    h.sample_rate   = read_s32le(0x10, sf);
    // 14: bitrate (not always accurate?)
    uint32_t config1 = read_u32le(0x18, sf); // block size + bps
    uint32_t config2 = read_u32le(0x1c, sf); // usually 0
    // 20: 0x10 padding (Xbox), partial DSP header on DSP or data

    if (h.channels != 1) // not seen (late games use .sfx + .2.sfx dual tracks)
        return NULL;
    h.stream_offset = head_size;

    // .sf0 mini files
    if (codec == 0x00 && extra_flag == 0 && head_size <= 0x20) {
        codec = 0x0002;
        h.loop_flag = 0;
    }

    switch (codec) {
        case 0xCFFF: // PS2 games
            if (config1 != 0x00040002 || config2 != 0)
                return NULL;
            h.coding = coding_PSX;
            h.layout = layout_none;

            h.num_samples = ps_bytes_to_samples(data_size, h.channels);
            break;

        case 0x0069: // Xbox games
            if (config1 != 0x00040024 || config2 != 0x00400002)
                return NULL;
            h.coding = coding_XBOX_IMA;
            h.layout = layout_none;

            h.num_samples = xbox_ima_bytes_to_samples(data_size, h.channels);
            break;

        case 0x0001: // GC games
            if (config1 != 0x00100002 || config2 != 0)
                return NULL;
            h.coding = coding_PCM16LE; //LE!
            h.layout = layout_none;

            h.num_samples = pcm16_bytes_to_samples(data_size, h.channels);
            break;

        case 0x0000: // Wii games
            if (config1 != 0x00100000 || config2 != 0)
                return NULL;
            h.coding = coding_NGC_DSP;
            h.layout = layout_none;

            h.num_samples = dsp_bytes_to_samples(data_size, h.channels);

            h.big_endian = true;
            h.coefs_offset = 0x3c;
            break;

        case 0x0002: // fake codec for .sf0 [ExciteBots (Wii)]
            if (config1 != 0x00100000 || config2 != 0)
                return NULL;
            h.coding = coding_PCM16BE;
            h.layout = layout_none;

            h.num_samples = pcm16_bytes_to_samples(data_size, h.channels);
            break;

        default:
            return NULL;
    }

    // full loops only
    h.loop_start = 0;
    h.loop_end = h.num_samples;

    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}

/* SFX0 - from early Monster Games [NASCAR Heat 2002 (PS2)] */
VGMSTREAM* init_vgmstream_sfx0_monster_old(STREAMFILE* sf) {

    /* checks*/
    uint32_t data_size = read_u32le(0x00,sf);
    uint32_t head_size = 0x16;
    if (!data_size || data_size + head_size != get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "sfx"))
        return NULL;

    // SFX0 is the internal fourCC used for .sfx

    meta_header_t h = {0};
    h.meta = meta_SFX0_MONSTER;

    int codec       = read_u16le(0x04, sf);
    h.channels      = read_u16le(0x06, sf);
    h.sample_rate   = read_s32le(0x08, sf);
    // 0c: bitrate (not always accurate?)
    uint32_t config1 = read_u32le(0x10, sf);
    uint16_t config2 = read_u16le(0x14, sf);

    if (h.channels != 1) //not seen
        return NULL;
    h.stream_offset = head_size;

    switch (codec) {
        case 0xCFFF:
            if (config1 != 0x00040002 || config2 != 0x6164)
                return NULL;
            h.coding = coding_PSX;
            h.layout = layout_none;

            h.num_samples = ps_bytes_to_samples(data_size, h.channels);

            h.loop_flag = read_u8(0x17, sf) == 0x06; //PSX loop flags
            break;

        default:
            return NULL;
    }

    // full loops only
    h.loop_start = 0;
    h.loop_end = h.num_samples;

    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
