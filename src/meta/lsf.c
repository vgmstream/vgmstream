#include "meta.h"
#include "../coding/coding.h"

/* .lsf - from Gizmondo Studios Helsingborg/Atod AB games [Chicane Street Racing (Gizmondo), Fastlane Street Racing (iOS)] */
VGMSTREAM* init_vgmstream_lsf_n1nj4n(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size, loop_start = 0, loop_end = 0;
    int channels, loop_flag, sample_rate;
    bool is_v2 = false;


    /* checks */
    if ((read_u64be(0x00, sf) & 0xFFFFFFFFFFFFFF00) == get_id64be("!n1nj4n\0"))
        is_v2 = false;
    else if ((read_u64be(0x00, sf) & 0xFFFFFFFFFFFFFF00) == get_id64be("n1nj4n!\0"))
        is_v2 = true; /* some files in Agaju: The Sacred Path (Gizmondo) */
    else
        return NULL;

    /* .lsf: actual extension, exe strings seem to call this format "LSF" as well */
    if (!check_extensions(sf, "lsf"))
        return NULL;

    uint8_t flags = read_u8(0x07, sf);
    uint32_t offset = 0x08;

    if (flags & 0x01) {
        loop_start = read_u32le(offset + 0x00,sf);
        loop_end = read_u32le(offset + 0x04,sf);
        offset += 0x08;
    }

    if ((flags & 0x01) && (flags & 0x02)) {
        //00: loop hist related?
        offset += 0x04;
    }

    if (flags & 0x02) {
        int count = read_u32le(offset + 0x00,sf); /* not channels */
        // per entry:
        //  00: channel related?
        //  04: null?
        //  0c: ~0x3130?
        offset += 0x04 + 0x0c * count;
    }

    sample_rate = read_u32le(offset + 0x00, sf);
    data_size = read_u32le(offset + 0x04, sf);
    offset += 0x08;
    start_offset = offset;

    if (start_offset + data_size != get_streamfile_size(sf))
        goto fail;

    channels = 1;
    loop_flag = loop_end > 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LSF_N1NJ4N;
    vgmstream->sample_rate = sample_rate;

    if (is_v2) {
        vgmstream->coding_type = coding_PSX_cfg;
        vgmstream->layout_type = layout_none;
        vgmstream->frame_size = 0x10;
    }
    else {
        /* custom codec but basically obfuscted PSX-cfg */
        vgmstream->coding_type = coding_LSF;
        vgmstream->layout_type = layout_interleave; //TODO: flat but decoder doesn't handle it
        vgmstream->interleave_block_size = 0x1c;
        vgmstream->frame_size = 0x1c; /* fixed but used below */
    }

    vgmstream->num_samples = ps_cfg_bytes_to_samples(data_size, vgmstream->frame_size, channels);
    vgmstream->loop_start_sample = ps_cfg_bytes_to_samples(loop_start, vgmstream->frame_size, channels);
    vgmstream->loop_end_sample = ps_cfg_bytes_to_samples(loop_end, vgmstream->frame_size, channels);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
