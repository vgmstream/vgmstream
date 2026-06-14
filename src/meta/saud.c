#include "meta.h"
#include "../util/meta_utils.h"


/* SAUD - from LucasArts games [Star Wars: Rebel Assault (PC), Mortimer and the Riddles of the Medallion (PC), Jar Jar's Journey Adventure Book (PC)] */
VGMSTREAM* init_vgmstream_saud(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "SAUD"))
        return NULL;
    if (!check_extensions(sf,"sad"))
        return NULL;

    // 04: chunk size
    if (!is_id32be(0x08, sf, "STRK"))
        return NULL;
    uint32_t strk_size = read_u32be(0x0c,sf);
    if (strk_size < 0x0c)
        return NULL;

    meta_header_t h = {0};

    // Some info from ScummVM: https://github.com/scummvm/scummvm/blob/master/engines/scumm/insane/rebel/rebel_audio.cpp
    // Smush videos (ANIM/SANM) may contain multiple SAUD chunks, but separate SAUD don't seem to do that

    /* parse STRK's ops */
    uint32_t offset = 0x10;
    uint32_t codec_offset = 0x10 + strk_size;
    while (offset < codec_offset) {
        uint8_t op_type = read_u8(offset + 0x00, sf);
        uint8_t op_size = read_u8(offset + 0x01, sf);
        offset += 0x02;

        // optional break op when chunk ends
        if (op_type == 0)
            break;

        switch (op_type) {
            case 0x01: // init (may appear multiple times with section(?) offsets, no apparent effect)
                // 0x00: start offset (within stream)
                // 0x04: end offset
                h.sample_rate = 22050;
                h.channels = 1;
                break;

            case 0x06: // set offset (same as init? found in later games)
                // 0x00: start offset
                // 0x04: end offset
                h.sample_rate = read_s32be(offset + 0x08, sf);
                h.channels = 1;
                break;

            default: // most others ops have a 32-bit number/config
                //VGM_LOG("SAUD: unknown op_type %x\n", op_type);
                break;
        }

        offset += op_size;
    }

    uint32_t type = read_u32be(codec_offset + 0x00,sf);
    h.stream_size = read_u32be(codec_offset + 0x04,sf);
    h.stream_offset = codec_offset + 0x08;
    // there may be a footer after data

    switch(type) {
        case 0x53444154: // SDAT
            h.coding = coding_PCM8_U;
            h.layout = layout_none;
            h.num_samples = pcm8_bytes_to_samples(h.stream_size, h.channels);
            break;

        case 0x5344364D: // SD6M
            h.coding = coding_PCM16LE_U;
            h.layout = layout_none;
            h.num_samples = pcm16_bytes_to_samples(h.stream_size, h.channels);
            break;

        default:
            VGM_LOG("SAUD: unknown codec %x\n", type);
            return NULL;
    }

    h.sf = sf;
    h.open_stream = true;

    h.meta = meta_SAUD;
    return alloc_metastream(&h);
}
