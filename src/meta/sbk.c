#include "meta.h"
#include "../coding/coding.h"

/* .SBK - from Addiction Pinball (PC) */
VGMSTREAM *init_vgmstream_sbk(STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    uint32_t sound_offset, sound_size, padding_size, sample_rate;
    uint16_t format, channels, block_size, bps;
    off_t table_offset, data_offset, entry_offset, start_offset;
    size_t table_size, data_size;
    int target_subsong = sf->stream_index, total_subsongs, loop_flag;

    /* checks */
    if (!check_extensions(sf, "sbk"))
        goto fail;

    /* check header */
    if (read_u32be(0x00, sf) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_u32be(0x08, sf) != 0x53424E4B) /* "SBNK" */
        goto fail;

    if (!find_chunk_le(sf, 0x57415649, 0x0c, 0, &table_offset, &table_size)) /* "WAVI" */
        goto fail;

    total_subsongs = table_size / 0x38;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;

    entry_offset = table_offset + 0x38 * (target_subsong - 1);
    sound_offset = read_u32le(entry_offset + 0x04, sf);
    sound_size = read_u32le(entry_offset + 0x00, sf);
    padding_size = read_u32le(entry_offset + 0x10, sf);
    sound_offset += padding_size;
    sound_size -= padding_size;

    /* read fmt chunk */
    format = read_u16le(entry_offset + 0x1c, sf);
    channels = read_u16le(entry_offset + 0x1e, sf);
    sample_rate = read_u32le(entry_offset + 0x20, sf);
    block_size = read_u16le(entry_offset + 0x28, sf);
    bps = read_u16le(entry_offset + 0x2a, sf);

    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_SBK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = sound_size;
    vgmstream->num_streams = total_subsongs;

    switch (format) {
        case 0x01: /* PCM */
            if (bps != 8 && bps != 16)
                goto fail;

            vgmstream->coding_type = (bps == 8) ? coding_PCM8_U : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (bps == 8) ? 0x01 : 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(sound_size, channels, bps);

            if (!find_chunk_le(sf, 0x57415644, 0x0c, 0, &data_offset, &data_size)) /* "WAVD" */
                goto fail;

            start_offset = data_offset + sound_offset;
            break;
        case 0x11: /* Microsoft IMA */
            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = block_size;
            vgmstream->num_samples = ms_ima_bytes_to_samples(sound_size, block_size, channels);

            if (!find_chunk_le(sf, 0x53574156, 0x0c, 0, &data_offset, &data_size)) /* "SWAV" */
                goto fail;

            start_offset = data_offset + sound_offset;
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
