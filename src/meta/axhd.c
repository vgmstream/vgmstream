#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"


 /* AXHD - Anges Studios bank format [Red Dead Revolver (Xbox), Spy Hunter 2 (Xbox)] */
VGMSTREAM* init_vgmstream_axhd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "AXHD"))
        return NULL;
    if (read_u8(0x04, sf) != 0x82) //version?
        return NULL;

    /* .xhd+xbd: from bigfiles */
    if (!check_extensions(sf, "xhd"))
        return NULL;

    meta_header_t h = {
        .meta = meta_AXHD,
    };

    h.target_subsong = sf->stream_index;
    if (h.target_subsong == 0)
        h.target_subsong = 1;

    // bank format somewhat like hd+bd from the PS2 versions
    int codec = 0;
    uint32_t table1_offset = 0x05;

    // base sections (typically only 1)
    int sections = read_u8(table1_offset,sf);
    table1_offset += 0x01;
    for (int i = 0; i < sections; i++) {
        uint32_t table2_offset = read_u16le(table1_offset, sf);
        table1_offset += 0x02;

        // entries per section (usually 1 per subsong)
        uint32_t subsections = read_u8(table2_offset, sf);
        // 01: flags?
        table2_offset += 0x01 + 0x04;
        for (int j = 0; j < subsections; j++) {
            uint32_t table3_offset = read_u16le(table2_offset, sf);
            table2_offset += 0x02;

            int sounds = read_u8(table3_offset, sf);
            // 01: flags?
            // 05: subflags?
            table3_offset += 0x01 + 0x04 + 0x02;
            for (int k = 0; k < sounds; k++) {
                uint32_t sound_offset = read_u16le(table3_offset, sf);
                table3_offset += 0x02;

                h.total_subsongs++;
                if (h.target_subsong != h.total_subsongs)
                    continue;

                h.stream_offset = read_u32le(sound_offset + 0x00, sf);
                // 04: flags (volume/pitch related?) + info?
                int fmt_size    = read_u8(sound_offset + 0x21, sf);
                h.stream_size   = read_u32le(sound_offset + 0x22, sf);
                if (fmt_size == 0)  { //dummy entry
                    codec = 0;
                    h.channels = 1;
                    h.sample_rate = 44100;
                    continue;
                }

                // fmt
                codec           = read_u16le(sound_offset + 0x26, sf);
                h.channels      = read_u16le(sound_offset + 0x28, sf);
                h.sample_rate   = read_s32le(sound_offset + 0x2a, sf);
                // 2e: average bitrate
                // 32: block size
                // 34: bits

                //TODO: this format repeats streams offsets for different entries
            }
        }
    }

    switch (codec) {
        case 0x00:
            h.coding = coding_SILENCE;
            h.layout = layout_none;
            h.num_samples = h.sample_rate;
            break;
        case 0x01:
            h.coding = coding_PCM16LE;
            h.interleave = 0x02;
            h.layout = layout_interleave;
            h.num_samples = pcm16_bytes_to_samples(h.stream_size, h.channels);
            break;
        case 0x69:
            h.coding = coding_XBOX_IMA;
            h.layout = layout_none;
            h.num_samples = xbox_ima_bytes_to_samples(h.stream_size, h.channels);
            break;
        default:
            goto fail;
    }
    h.open_stream = true;
    h.has_subsongs = true;

    h.sf_head = sf;
    h.sf_body = open_streamfile_by_ext(sf,"xbd");
    if (!h.sf_body) goto fail;

    vgmstream = alloc_metastream(&h);
    close_streamfile(h.sf_body);
    return vgmstream;
fail:
    close_streamfile(h.sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
