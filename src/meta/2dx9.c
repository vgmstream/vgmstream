#include "meta.h"
#include "../coding/coding.h"

/* 2DX9 - from Konami arcade games [beatmaniaIIDX16: EMPRESS (AC), BeatStream (AC), REFLEC BEAT (AC), Bishi Bashi Channel (AC)] */
VGMSTREAM* init_vgmstream_2dx9(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag;


    /* checks */
    if (!is_id32be(0x00,sf, "2DX9"))
        return NULL;

    // .2dx: container extension (with multiple 2dx9) and debug strings
    // .2dx9: header ID
    if (!check_extensions(sf, "2dx,2dx9"))
        return NULL;

    // 04: RIFF offset
    // 08: RIFF size
    // 0c: flags?
    // 10: samples related?
    // 14: loop start
    // 18: full RIFF (always MSADPCM w/ fact)

    if (!is_id32be(0x18,sf, "RIFF"))
        return NULL;
    if (!is_id32be(0x20,sf, "WAVE"))
        return NULL;
    if (!is_id32be(0x24,sf, "fmt "))
        return NULL;
    if (!is_id32be(0x6a,sf, "data"))
        return NULL;

    // some data loop from beginning to the end by hardcoded flag so cannot be recognized from sound file
    loop_flag = (read_s32le(0x14,sf) > 0);
    channels = read_s16le(0x2e,sf);
    start_offset = 0x72;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_2DX9;
    vgmstream->sample_rate = read_s32le(0x30,sf);
    vgmstream->num_samples = read_s32le(0x66,sf);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_u32le(0x14,sf) / 2 / channels;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->frame_size  = read_u16le(0x38,sf);
    if (!msadpcm_check_coefs(sf, 0x40))
        goto fail;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
