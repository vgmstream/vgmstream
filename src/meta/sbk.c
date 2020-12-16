#include "meta.h"
#include "../coding/coding.h"

/* .SBK - from Addiction Pinball (PC) */
VGMSTREAM *init_vgmstream_sbk(STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    uint32_t sound_offset, sound_size, padding_size, sample_rate;
    uint16_t format, channels, block_size, bps;
    off_t table_offset, data_offset, entry_offset, cfg_fmt_offset;
    size_t table_size, data_size, cfg_entry_size;
    int target_subsong = sf->stream_index, total_subsongs, loop_flag, is_streamed;

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

    if (find_chunk_le(sf, 0x53574156, 0x0c, 0, &data_offset, &data_size)) { /* "SWAV" */
        cfg_entry_size = 0x38;
        cfg_fmt_offset = 0x1c;
    } else {
        /* 1997 demo version with sound names and no streamed section */
        cfg_entry_size = 0x24;
        cfg_fmt_offset = 0x0c;
    }

    total_subsongs = table_size / cfg_entry_size;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;

    entry_offset = table_offset + cfg_entry_size * (target_subsong - 1);
    sound_offset = read_u32le(entry_offset + 0x04, sf);
    sound_size = read_u32le(entry_offset + 0x00, sf);
    if (cfg_entry_size == 0x38) {
        padding_size = read_u32le(entry_offset + 0x10, sf);
        sound_offset += padding_size;
        sound_size -= padding_size;

        is_streamed = read_u8(entry_offset + 0x36, sf);
    } else {
        is_streamed = 0;
    }

    if (!is_streamed) {
        if (!find_chunk_le(sf, 0x57415644, 0x0c, 0, &data_offset, &data_size)) /* "WAVD" */
            goto fail;
    } else {
        if (!find_chunk_le(sf, 0x53574156, 0x0c, 0, &data_offset, &data_size)) /* "SWAV" */
            goto fail;
    }

    sound_offset += data_offset;

    /* read fmt chunk */
    format = read_u16le(entry_offset + cfg_fmt_offset + 0x00, sf);
    channels = read_u16le(entry_offset + cfg_fmt_offset + 0x02, sf);
    sample_rate = read_u32le(entry_offset + cfg_fmt_offset + 0x04, sf);
    block_size = read_u16le(entry_offset + cfg_fmt_offset + 0x0c, sf);
    bps = read_u16le(entry_offset + cfg_fmt_offset + 0x0e, sf);

    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_SBK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = sound_size;
    vgmstream->num_streams = total_subsongs;

    if (cfg_entry_size == 0x24) {
        uint32_t num_entries, i;

        if (!find_chunk_le(sf, 0x544F4320, 0x0c, 0, &table_offset, &table_size)) /* "TOC " */
            goto fail;

        num_entries = table_size / 0x10;
        for (i = 0; i < num_entries; i++) {
            entry_offset = table_offset + 0x10 * i;

            if ((read_u8(entry_offset + 0x01, sf) & 0x80) &&
                read_u8(entry_offset + 0x00, sf) == (target_subsong - 1)) {
                read_string(vgmstream->stream_name, 0x0c, entry_offset + 0x04, sf);
                break;
            }
        }
    }

    switch (format) {
        case 0x01: /* PCM */
            if (bps != 8 && bps != 16)
                goto fail;

            vgmstream->coding_type = (bps == 8) ? coding_PCM8_U : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (bps == 8) ? 0x01 : 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(sound_size, channels, bps);
            break;
        case 0x11: /* Microsoft IMA */
            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = block_size;
            vgmstream->num_samples = ms_ima_bytes_to_samples(sound_size, block_size, channels);
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, sound_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
