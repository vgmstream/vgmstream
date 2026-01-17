#include "meta.h"
#include "../util/meta_utils.h"
#include "../util/endianness.h"
#include "../coding/coding.h"


/* .bwav - from Warthog games [Harry Potter and the Sorcerer's Stone (multi), Animaniacs: The Great Edgar Hunt (multi)] */
VGMSTREAM* init_vgmstream_bwav_warthog(STREAMFILE* sf) {

    /* checks*/
    uint32_t epoch = read_u32le(0x00,sf); // linux time, always LE
    if (epoch < 0x386D4380 || epoch > 0x43B71B80) // 2000-2006
        return NULL;
    if (!check_extensions(sf, "bwav"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_BWAV_WARTHOG;

    h.big_endian = guess_endian32(0x0c, sf);
    read_u32_t read_u32 = get_read_u32(h.big_endian);
    read_s32_t read_s32 = get_read_s32(h.big_endian);
    read_u16_t read_u16 = get_read_u16(h.big_endian);

    // 04: ? (LE 01=GC/Xbox, 02=PS2)
    // 08: codec LE (3D8D9F9A=DSP, A9486763/87DC9603=PSX, 11EC9174=XIMA)
    // 0c: always 1?
    int header_size  = read_u32(0x10, sf);
    int header_size2 = read_u32(0x14, sf);
    if (header_size != header_size2)
        return NULL;

    switch(header_size) {
        case 0x20:
            // 18: related to samples?
            h.stream_size = read_u32(0x1c, sf);
            // 20: stream size (with padding)
            // 24: null
            // 28: null
            // 2c: null
            h.num_samples = read_s32(0x30, sf);
            h.sample_rate = read_u16(0x34, sf);
            //0x36: sample rate repeat
            h.channels = read_s32(0x38, sf); //assumed
            // 0x3c: stream size (with padding)
            // 0x40: stream size (with padding)
            h.stream_offset = 0x44;

            h.coding = coding_PSX;
            h.layout = layout_none;
            break;

        case 0x28:
            // 18: always 2?
            // 1c: always 1?
            h.sample_rate = read_s32(0x20, sf);
            // 24: null
            // 28: bits per sample
            // 2c: null
            h.stream_size = read_u32(0x30, sf);
            // 34: null
            // 38: stream size
            // 3c: config?
            h.channels = read_s32(0x40, sf); //assumed
            // 44: stream size
            // 48: stream size
            h.stream_offset = 0x4c;

            h.num_samples = xbox_ima_bytes_to_samples(h.stream_size, h.channels);
            h.coding = coding_XBOX_IMA;
            h.layout = layout_none;
            break;

        case 0x54:
            // 18: always 1?
            // 1c: null
            // 20: nibbles
            // 24: nibbles
            // 28: null
            h.sample_rate = read_s32(0x2c, sf);
            h.num_samples = read_s32(0x30, sf);
            h.stream_size = read_u32(0x34, sf);
            h.coefs_offset = 0x38;
            h.hists_offset = h.coefs_offset + 0x24;
            // 68: config?
            h.channels = read_s32(0x6c, sf); //assumed
            // 70: stream size
            // 74: stream size
            h.stream_offset = 0x78;

            h.coding = coding_NGC_DSP;
            h.layout = layout_none;
            break;

        default:
            return NULL;
    }

    if (h.channels != 1) // not seen
        return NULL;

    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
