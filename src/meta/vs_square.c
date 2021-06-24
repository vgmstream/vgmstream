#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* VS - VagStream from Square Sounds Co. games [Final Fantasy X (PS2) voices, Unlimited Saga (PS2) voices, All Star Pro-Wrestling 2/3 (PS2) music] */
VGMSTREAM* init_vgmstream_vs_square(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag, pitch, flags;
    off_t start_offset;


    /* checks */
    /* .vs: extension from debug strings (probably like The Bouncer's .vs, very similar) */
    if (!check_extensions(sf, "vs"))
        goto fail;
    if (!is_id32be(0x00,sf,"VS\0\0"))
        goto fail;

    flags = read_u32le(0x04,sf);
    /* 0x08: block number */
    /* 0x0c: blocks left in the subfile */
    pitch = read_u32le(0x10,sf); /* usually 0x1000 = 48000 */
    /* 0x14: volume, usually 0x64 = 100, up to 128 [Lethal Skies / Sidewinder F (PS2)] */
    /* 0x18: null */
    /* 0x1c: null */

    /* some Front Mission 4 voices have flag 0x100, no idea */
    if (flags != 0x00 && flags != 0x01) {
        VGM_LOG("VS: unknown flags %x\n", flags);
    }

    loop_flag = 0;
    channels = (flags & 1) ? 2 : 1;
    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VS_SQUARE;
    vgmstream->sample_rate = round10((48000 * pitch) / 4096); /* needed for rare files */
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_vs_square;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    /* calc num_samples */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += ps_bytes_to_samples(vgmstream->current_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        block_update(start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
