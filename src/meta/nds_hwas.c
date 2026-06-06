#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* HWAS - from Vicarious Visions games [Spider-Man 3 (DS), Tony Hawk's Downhill Jam (DS)] */
VGMSTREAM* init_vgmstream_hwas(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag = 0;

    /* checks */
    if (!is_id32be(0x00,sf, "sawh"))
        return NULL;
    /* .hwas: usually in archives but also found named (ex. Guitar Hero On Tour) */
    if (!check_extensions(sf,"hwas"))
        return NULL;

    loop_flag = 1; // almost all files seem to loop (usually if num_samples != loop_end doesn't loop, but not always)
    channels = read_s32le(0x0C,sf);
    if (channels > 1) goto fail; // unknown layout

    start_offset = 0x200;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_HWAS;
    vgmstream->sample_rate = read_s32le(0x08,sf);
    vgmstream->num_samples = ima_bytes_to_samples(read_u32le(0x14,sf), channels);
    vgmstream->loop_start_sample = ima_bytes_to_samples(read_u32le(0x10,sf), channels); //assumed, always 0
    vgmstream->loop_end_sample = ima_bytes_to_samples(read_u32le(0x18,sf), channels);

    vgmstream->coding_type = coding_IMA_mono;
    vgmstream->layout_type = layout_blocked_hwas;
    vgmstream->full_block_size = read_u32le(0x04,sf); // usually 0x2000, 0x4000 or 0x8000

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
