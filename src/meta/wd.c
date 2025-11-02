#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"
#include "../util/endianness.h"
#include "../util/spu_utils.h"


/* .WD - Square wave banks [Final Fantasy XI (PS2), Final Fantasy X-2 (PS2/Vita), FF Cristal Chronicles (GC)] */
VGMSTREAM* init_vgmstream_wd(STREAMFILE* sf) {

    /* checks */
    if ((read_u32be(0x00,sf) & 0xFFFF0000) != get_id32be("WD\0\0"))
        return NULL;
    if (!check_extensions(sf, "wd"))
        return NULL;

    bool big_endian = guess_endian32(0x04, sf);
    read_u32_t read_u32 = get_read_u32(big_endian);
    read_s32_t read_s32 = get_read_s32(big_endian);

    // 02: file id
    uint32_t data_size  = read_u32(0x04, sf);
    int intruments      = read_s32(0x08, sf);
    int waves           = read_s32(0x0c, sf);
    // 0x10: usually null, low size in The Bouncer
    // 0x14: padded until 0x20
    // 0x20: instrument table
    // 0xNN: wave headers

    if (read_u32be(0x14, sf) != 0 || read_u32be(0x18, sf) != 0 || read_u32be(0x1c, sf) != 0)
        return NULL;

    // seemingly 'instruments' are main entries with N sub-waves (marked with flags), though often 1:1
    if (intruments > waves || waves > 0x200) // arbitrary max (~0x100 seems rare)
        return NULL;


    meta_header_t h = {0};
    h.meta  = meta_WD;

    h.target_subsong = sf->stream_index;
    h.total_subsongs = waves;
    if (!check_subsongs(&h.target_subsong, h.total_subsongs))
        return NULL;


    uint32_t entry_size = big_endian ? 0x60 : 0x20;
    // usually 0x20 + intruments * 0x04 padded to 0x10 but sometimes more, use first offset
    uint32_t waves_offset = read_u32(0x20, sf);
    uint32_t head_offset = waves_offset + (h.target_subsong - 1) * entry_size;
    uint32_t data_offset = waves_offset + waves * entry_size;

    // typically matches, but in FF X-2 Vita it's a bit off
    if (data_size < 0x40 && data_offset + data_size + 0x100 < get_streamfile_size(sf))
        return NULL;

    // some info from:
    // - https://github.com/BlackFurniture/ffcc/blob/master/ffcc/audio.py
    // - https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/SquarePS2/WD.cpp
    if (big_endian) {
        //00: padding?
        //01: frame marker?
        //02: flags (first / last sub-wave?)
        //03: stereo?
        h.stream_offset     = read_u32(head_offset + 0x04, sf); //within data
        //08: -1 or value (loop start nibble?)
        //0c: nibbles (loop end?)
        h.stream_size       = read_u32(head_offset + 0x10, sf);
        int32_t key         = read_s32(head_offset + 0x14, sf); //8.24 key notation
        //18: key high?
        //19: velocity?
        //1a: volume?
        //1b: pan?
        //1c: null
        //22: DSP coefs + hist
        //XX: ADSR config
        h.coefs_offset = head_offset + 0x22;

        h.channels      = 1;
        h.sample_rate   = square_key_to_sample_rate(key, 32000);
    }
    else {
        //00: flags? supposedly: 00=stereo (paired with next sample), 01=first/last region in multi-samples
        h.stream_offset     = read_u32(head_offset + 0x04, sf); //within data
        h.loop_start        = read_u32(head_offset + 0x08, sf);
        //0c: ADSR config?
        int32_t key         = read_s32(head_offset + 0x10, sf); //8.24 key notation
        //14: key high?
        //15: velocity?
        //16: volume?
        //17: pan?
        //18: null
        //1c: null

        // oddity found in FFXI, all offsets add 0x0C
        if (h.stream_offset % 0x10) {
            h.stream_offset = h.stream_offset & 0xFFFFFF00;
        }

        h.channels      = 1;
        h.sample_rate   = square_key_to_sample_rate(key, 48000);
    }

    h.stream_offset += data_offset;

    if (h.stream_size == 0) {
        // PS2 has no sizes, presumably since PS-ADPCM stops using flags.
        // Rarely offsets are not ordered (FF XI), so find next offset within headers
        uint32_t next_offset = get_streamfile_size(sf);
        uint32_t head_offset = waves_offset;
        for (int i = 0; i < waves; i++) {
            uint32_t temp_offset = read_u32(head_offset + 0x04, sf);
            head_offset += entry_size;

            if (temp_offset > h.stream_offset && temp_offset < next_offset) {
                next_offset = temp_offset & 0xFFFFFF00;
            }
        }

        h.stream_size = next_offset - h.stream_offset;
    }


    //TODO: seems like an offset but not correct
    //h.loop_flag = h.loop_start > 0;

    if (big_endian) {
        h.coding = coding_NGC_DSP;
        h.num_samples   = dsp_bytes_to_samples(h.stream_size, h.channels);
        h.loop_start    = dsp_bytes_to_samples(h.loop_start, h.channels);
        h.loop_end = h.num_samples;
    }
    else {
        h.num_samples   = ps_bytes_to_samples(h.stream_size, h.channels);
        h.loop_start    = ps_bytes_to_samples(h.loop_start, h.channels);
        h.loop_end = h.num_samples;
        h.coding = coding_PSX;
    }

    h.big_endian = big_endian;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
