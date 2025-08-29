#include "meta.h"
#include "../layout/layout.h"
#include "../util/endianness.h"

/* THP - Nintendo movie format found in GC/Wii games */
VGMSTREAM* init_vgmstream_thp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "THP\0"))
        return NULL;
    /* .thp: actual extension
     * .dsp: fake extension?
     * .mov: Dora the Explorer: JttPP (GC)
     * (extensionless): Fragile (Wii) */
    if (!check_extensions(sf, "thp,dsp,mov,"))
        return NULL;

    bool big_endian = guess_endian32(0x08,sf); // Pikmin 2 (Switch) uses LE
    read_u32_t read_u32 = big_endian ? read_u32be : read_u32le;


    uint32_t version = read_u32(0x04,sf); // 16b+16b major/minor
    // 0x08: max buffer size
    uint32_t max_audio_size = read_u32(0x0C,sf);
    // 0x10: fps in float
    // 0x14: block count
    uint32_t first_block_size = read_u32(0x18,sf);
    // 0x1c: data size

    if (version != 0x00010000 && version != 0x00011000) // v1.0 (~2002) or v1.1 (rest)
        return NULL;
    if (max_audio_size == 0) /* no sound */
        return NULL;

    uint32_t component_type_offset = read_u32(0x20,sf);
    // 0x24: block offsets table offset (optional, for seeking)
    start_offset = read_u32(0x28,sf);
    // 0x2c: last block offset

    /* first component "type" x16 then component headers */
    int num_components = read_u32(component_type_offset,sf);
    component_type_offset += 0x04;
    uint32_t component_data_offset = component_type_offset + 0x10;

    /* parse "component" (data that goes into blocks) */
    for (int i = 0; i < num_components; i++) {
        int type = read_u8(component_type_offset + i,sf);

        if (type == 0x00) { /* video */
            if (version == 0x00010000)
                component_data_offset += 0x08; /* width + height */
            else
                component_data_offset += 0x0c; /* width + height + format? */
        }
        else if (type == 0x01) { /* audio */
            /* parse below */
#if 0
            if (version == 0x00010000)
                component_data_offset += 0x0c; /* channels + sample rate + samples */
            else
                component_data_offset += 0x10; /* channels + sample rate + samples + format? */
#endif
            break;
        }
        else { /* 0xFF / no data (reserved as THP is meant to be extensible) */
            goto fail;
        }
    }

    /* official docs remark original's audio is adjusted to match GC's hardware rate
     * (48000 > 48043 / 32000 > 32028), not sure if that means ouput sample rate should
     * adjusted, but we can't detect Wii (non adjusted) .thp tho */

    loop_flag = 0;
    channels = read_u32(component_data_offset + 0x00,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_u32(component_data_offset + 0x04,sf);
    vgmstream->num_samples = read_u32(component_data_offset + 0x08,sf);

    vgmstream->meta_type = meta_THP;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_thp;
    vgmstream->codec_endian = big_endian;
    /* coefs are in every block */

    vgmstream->full_block_size = first_block_size; // save block size for block layout

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
