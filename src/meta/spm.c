#include "meta.h"
#include "../coding/coding.h"


/* SPM - Seq-PCM stream Square Sounds Co. games [Lethal Skies Elite Pilot: Team SW (PS2)] */
VGMSTREAM* init_vgmstream_spm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;
    size_t data_size;
    int32_t loop_start, loop_end;


    /* checks */
    /* .spm: extension from debug strings */
    if (!check_extensions(sf, "spm"))
        goto fail;
    if (!is_id32be(0x00,sf,"SPM\0"))
        goto fail;

    data_size  = read_u32le(0x04,sf);
    loop_start = read_s32le(0x08,sf);
    loop_end   = read_s32le(0x0c,sf);
    /* 0x10: volume? */
    /* rest: null */
    start_offset = 0x20;

    channels = 2;
    loop_flag = 1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SPM;
    vgmstream->sample_rate = 48000;

    vgmstream->num_samples = pcm16_bytes_to_samples(data_size - start_offset, channels);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
