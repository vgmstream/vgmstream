#include "meta.h"
#include "../coding/coding.h"
#include "../util/reader_text.h"
#include "../util/meta_utils.h"

static bool parse_agsc(meta_header_t* hdr, STREAMFILE* sf, int version);


/* .agsc - from Retro Studios games [Metroid Prime (GC), Metroid Prime 2 (GC)] */
VGMSTREAM* init_vgmstream_agsc(STREAMFILE* sf) {

    /* checks */
    int version;
    if (is_id32be(0x00, sf, "Audi"))
        version = 1;
    else if (read_u32be(0x00, sf) == 0x00000001)
        version = 2;
    else
        return NULL;

    /* .agsc: 'class' type in .pak */
    if (!check_extensions(sf, "agsc"))
        return NULL;


    meta_header_t hdr = {0};
    if (!parse_agsc(&hdr, sf, version))
        return NULL;

    hdr.meta = meta_AGSC;
    hdr.coding = coding_NGC_DSP;
    hdr.layout = layout_none;
    hdr.big_endian = true;
    hdr.allow_dual_stereo = true;

    hdr.sf = sf;
    hdr.open_stream = true;

    return alloc_metastream(&hdr);
}


static bool parse_agsc(meta_header_t* hdr, STREAMFILE* sf, int version) {
    uint32_t offset;
    int name_size;

    switch(version) {
        case 1: 
            // usually "Audio/" but rarely "Audio//"
            name_size = read_string(NULL, 0x20, 0x00, sf);
            if (name_size == 0) // not a string
                return false;
            offset = name_size + 1;
            break; 

        case 2:
            /* after fixed ID */
            offset = 0x04;
            break;

        default:
            return false;
    }

    /* after id starts with name + null */
    hdr->name_offset = offset;
    name_size = read_string(NULL, 0x20, offset, sf);
    if (name_size == 0) // not a string
        return false;
    offset += name_size + 1;

    uint32_t head_offset, data_offset;
    uint32_t unk1_size, unk2_size, head_size, data_size;
    switch(version) {
        case 1:
            /* per chunk: chunk size + chunk data */
            unk1_size = read_u32be(offset, sf);
            offset += 0x04 + unk1_size;

            unk2_size = read_u32be(offset, sf);
            offset += 0x04 + unk2_size;

            data_offset = offset + 0x04; // data chunk goes before headers...
            data_size = read_u32be(offset, sf);
            offset += 0x04 + data_size;

            head_offset = offset + 0x04;
            head_size = read_u32be(offset, sf);
            offset += 0x04 + head_size;

            break;

        case 2:
            /* chunk sizes per chunk + chunk data per chunk */
            offset += 0x02; // song id?
            unk1_size = read_u32be(offset + 0x00, sf);
            unk2_size = read_u32be(offset + 0x04, sf);
            head_size = read_u32be(offset + 0x08, sf);
            data_size = read_u32be(offset + 0x0c, sf);

            head_offset = offset + 0x10 + unk1_size + unk2_size;
            data_offset = head_offset + head_size;

            break;
        default:
            return false;
    }
    // offsets/values/data aren't 32b aligned but file is (0xFF padding)

    // possible in some test banks
    if (data_size == 0 || head_size < 0x20 + 0x28) {
        vgm_logi("AGSC: bank has no subsongs (ignore)\n");
        return false;
    }

    /* header chunk has 0x20 headers per subsong + 0xFFFFFFFF (end marker) + 0x28 coefs per subsongs,
     * no apparent count even in other chunks */
    hdr->total_subsongs = (head_size - 0x04) / (0x20 + 0x28);
    hdr->target_subsong = sf->stream_index;
    if (!check_subsongs(&hdr->target_subsong, hdr->total_subsongs))
        return false;

    uint32_t entry_offset = head_offset + 0x20 * (hdr->target_subsong - 1);

    // 00: id?
    hdr->stream_offset  = read_u32be(entry_offset + 0x04,sf) + data_offset;
    // 08: null?
    // 0c: always 0x3c00?
    hdr->sample_rate    = read_u16be(entry_offset + 0x0e,sf);
    hdr->num_samples    = read_s32be(entry_offset + 0x10,sf);
    hdr->loop_start     = read_s32be(entry_offset + 0x14,sf);
    hdr->loop_end       = read_s32be(entry_offset + 0x18,sf); // loop length
    hdr->coefs_offset   = read_s32be(entry_offset + 0x1c,sf);

    if (hdr->loop_end)
        hdr->loop_end = hdr->loop_end + hdr->loop_start - 1;
    hdr->loop_flag = hdr->loop_end != 0;

    hdr->coefs_offset += head_offset + 0x08; // skip unknown hist/loop ps-like values

    hdr->channels = 1; // MP2 uses dual stereo for title track
    hdr->stream_size = hdr->num_samples / 14 * 8 * hdr->channels; // meh

    return true;
}
