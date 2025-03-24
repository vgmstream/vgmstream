#include "meta.h"
#include "../coding/coding.h"


/* SD9 - from Konami arcade games [beatmania IIDX series (AC), BeatStream (AC)] */
VGMSTREAM* init_vgmstream_sd9(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00, sf, "SD9\0"))
        return NULL;
    // .sd9: header ID
    if (!check_extensions(sf, "sd9"))
        return NULL;

    // 04: header size
    // 08: data size
    // 0c: 0x3231?
    int loop_count = read_s16le(0x0e,sf); //-1 or N;
    uint32_t loop_start = read_u32le(0x14, sf);
    uint32_t loop_end = read_u32le(0x18, sf);
    // 1c: loop flag? (1=loop_end defined)
    // 1e: category id?

    // Some SD9s sets count > 0 without any loop points specificed, loop entire song.
    // However can't tell apart from songs that shouldn't do full loops. (ex. IIDX 16 sys_sound.ssp #3 vs #26).
    // In IIDX 21 loop count < 0 w/ loops exist; in other cases count < 0 usually has no loops defined.
    loop_flag = (loop_count > 0) || (loop_count < 0 && loop_end);

    // regular RIFF header with fmt + fact + data
    if (!is_id32be(0x20, sf, "RIFF"))
        return NULL;
    if (!is_id32be(0x28, sf, "WAVE"))
        return NULL;
    if (!is_id32be(0x2c, sf, "fmt "))
        return NULL;

    channels = read_u16le(0x36, sf);
    start_offset = 0x7a;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SD9;
    vgmstream->sample_rate = read_s32le(0x38, sf);
    vgmstream->num_samples = read_s32le(0x6e, sf);
    vgmstream->loop_start_sample = pcm16_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = pcm16_bytes_to_samples(loop_end, channels);
    if (vgmstream->loop_end_sample == 0)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->frame_size = read_u16le(0x40, sf);
    if (!msadpcm_check_coefs(sf, 0x48))
        goto fail;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
