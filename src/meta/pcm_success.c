#include "meta.h"
#include "../coding/coding.h"


/* PCM - from Success (related) games [Metal Saga (PS2), Tetris Kiwamemichi (PS2), Duel Masters: Rebirth of Super Dragon (PS2)] */
VGMSTREAM* init_vgmstream_pcm_success(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, interleave;
    size_t data_size, loop_start, loop_end, loop_adjust;


    /* checks */
    if (!check_extensions(sf, "pcm"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x50434D20) /* "PCM " */
        goto fail;
    if (read_u32le(0x04,sf) != 0x00010000) /* version? */
        goto fail;
    if (read_u32le(0x08,sf) + 0x8000 < get_streamfile_size(sf)) /* data size without padding */
        goto fail;

    interleave = 0x800;
    start_offset = 0x800;

    sample_rate = read_s32le(0x0c,sf);
    channels    = read_s32le(0x10,sf);
    loop_flag   = read_s32le(0x14,sf);

    data_size   = read_s32le(0x18,sf) * interleave * channels;
    /* loops seems slightly off, so 'adjust' meaning may need to be tweaked */
    loop_adjust = read_s32le(0x1c,sf) * channels; /* from 0..<0x800 */
    loop_start  = read_s32le(0x20,sf) * interleave * channels + loop_adjust;
    loop_adjust = read_s32le(0x24,sf) * channels; /* always 0x800 (0 if no loop flag) */
    loop_end    = read_s32le(0x28,sf) * interleave * channels + (interleave * channels - loop_adjust);

    /* 0x2c: always 1? */
    /* 0x30/40: padding garbage (also at file end) */

    /* not always accurate and has padding */
    if (data_size > get_streamfile_size(sf) - start_offset)
        data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PCM_SUCCESS;
    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
