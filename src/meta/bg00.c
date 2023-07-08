#include "meta.h"
#include "../coding/coding.h"

/* BG00 - from Cave games [Ibara (PS2), Mushihime-sama (PS2)] */
VGMSTREAM* init_vgmstream_bg00(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag = 0;


    /* check */
    if (!is_id32be(0x00,sf, "BG00"))
        return NULL;

    /* .bg00: header ID (no filenames or debug strings) */
    if (!check_extensions(sf,"bg00"))
        return NULL;

    if (!is_id32be(0x40,sf, "VAGp"))
        return NULL;
    if (!is_id32be(0x70,sf, "VAGp"))
        return NULL;

    loop_flag = 0; /* flag at 0x08? loop points seem external */
    channels = 2; /* mono files use regular VAG */
    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BG00;
    vgmstream->sample_rate = read_s32be(0x80,sf);
    vgmstream->num_samples = ps_bytes_to_samples(read_32bitBE(0x4C,sf), 1);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x10,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
