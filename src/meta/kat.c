#include "meta.h"
#include "../coding/coding.h"

/* .KAT - standard sound bank format used on Dreamcast */
VGMSTREAM *init_vgmstream_kat(STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    uint32_t entry_offset, type, start_offset, data_size, sample_rate, channels, bit_depth, loop_start, loop_end;
    int loop_flag;
    int num_sounds, target_stream = sf->stream_index;

    /* checks */
    if (!check_extensions(sf, "kat"))
        goto fail;

    num_sounds = read_u32le(0x00, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = 0x04 + (target_stream - 1) * 0x2c;

    type = read_u32le(entry_offset + 0x00, sf);
    if (type != 0x01) /* only type 0x01 is supported, other types are MIDI, programs, etc */
        goto fail;

    bit_depth = read_u32le(entry_offset + 0x14, sf);
    if (bit_depth != 4 && bit_depth != 8 && bit_depth != 16)
        goto fail;

    start_offset = read_u32le(entry_offset + 0x04, sf);
    data_size = read_u32le(entry_offset + 0x08, sf);
    sample_rate = read_u32le(entry_offset + 0x0c, sf);
    if (sample_rate > 48000)
        goto fail;

    loop_flag = read_u32le(entry_offset + 0x10, sf);
    loop_start = read_u32le(entry_offset + 0x1c, sf);
    loop_end = read_u32le(entry_offset + 0x20, sf);

    channels = 1; /* mono only */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_KAT;
    vgmstream->sample_rate = sample_rate;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->stream_size = data_size;
    vgmstream->num_streams = num_sounds;

    switch (bit_depth) {
        case 4:
            vgmstream->coding_type = coding_AICA_int;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = yamaha_bytes_to_samples(data_size, channels);
            break;
        case 8:
            vgmstream->coding_type = coding_PCM8;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 8);
            break;
        case 16:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
