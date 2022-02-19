#include "meta.h"
#include "../coding/coding.h"

/* .MIC - from KOEI games [Crimson Sea 2 (PS2), Dynasty Tactics 2 (PS2)] */
VGMSTREAM* init_vgmstream_mic_koei(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, loop_start, loop_end, sample_rate;
    size_t interleave, block_size;


    /* checks */
    if (!check_extensions(sf, "mic"))
        goto fail;

    /* simple header so throws some extra in some checks */
    start_offset    = read_u32le(0x00,sf);
    if (start_offset != 0x800) goto fail;
    sample_rate     = read_u32le(0x04,sf);
    channels        = read_u32le(0x08,sf);
    if (channels > 4) goto fail; /* 1/2/4 are known */
    interleave      = read_u32le(0x0c,sf);
    if (interleave != 0x10) goto fail;

    loop_end        = read_s32le(0x10,sf);
    loop_start      = read_s32le(0x14,sf);
    if (read_u32le(0x18,sf) != 0) goto fail;
    if (read_u32le(0x1c,sf) != 0) goto fail;

    loop_flag     = (loop_start != 1);
    block_size    = interleave * channels;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_MIC;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_PSX;
    vgmstream->interleave_block_size = interleave;
    vgmstream->layout_type = layout_interleave;

    vgmstream->num_samples = ps_bytes_to_samples(loop_end * block_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start * block_size, channels);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
