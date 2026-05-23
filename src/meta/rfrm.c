#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* RFTM - Retro Studios format */
VGMSTREAM* init_vgmstream_rfrm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "RFRM"))
        return NULL;
    // 0x04: chunk size (64b), from 0x20
    // 0x0c: chunk version? (64b)

    if (!check_extensions(sf, "csmp"))
        return NULL;

    /* base header */ 
    if (!is_id32be(0x14, sf, "CSMP"))
        return NULL;
    int version = read_s32be(0x18,sf);
    if (version > 0xFF) 
        version = read_s32le(0x18,sf); // 0x1F+
    // 0x1c: version again? (+1 in 0x2E)
    // 0x20+: chunks

    switch(version) {
        case 0x0a: // Donkey Kong Country: Tropical Freeze (Wii U)
        case 0x11: // Harmony / RS11 (Switch)
        case 0x12: // Donkey Kong Country: Tropical Freeze (Switch)
        case 0x1F: // Metroid Prime Remastered (Switch)
        case 0x2E: // Metroid Prime 4: Beyond (Switch)
            break;
        default:
            return NULL;
    }

    bool big_endian = (version == 0x0a);
    read_u32_t read_u32 = get_read_u32(big_endian);
    read_s32_t read_s32 = get_read_s32(big_endian);
    read_u16_t read_u16 = get_read_u16(big_endian);

    off_t fmta_offset = 0, ras3_offset = 0, data_offset = 0;
    size_t data_size = 0;

    /* parse chunks */
    {
        // 0x11/12 have different endianness vs other fields
        read_u64_t read_u64ce = (version >= 0x1f) ? read_u64le : read_u64be;
        off_t chunk_offset = 0x20;
        off_t file_size = get_streamfile_size(sf);

        while (chunk_offset < file_size) {
            uint32_t chunk_type =           read_u32be(chunk_offset+0x00,sf);
            uint32_t chunk_size = (uint32_t)read_u64ce(chunk_offset+0x04,sf);
            // 0x0c: chunk version? (0/1)

            switch(chunk_type) {
                case 0x464D5441: // "FMTA"
                    fmta_offset = chunk_offset + 0x18;
                    break;
                case 0x52415333: // "RAS3" (version 0x1f+)
                    ras3_offset = chunk_offset + 0x18;
                    break;
                case 0x44415441: // "DATA"
                    data_offset = chunk_offset + 0x18;
                    data_size = chunk_size;
                    if (version >= 0x1f) {
                        // force end, after DATA there is 0x04 padding and a RFRM + FOOT chunk
                        chunk_offset = file_size;
                    }
                    break;
                // "CRMS": usually before "DATA" (MPR)
                // "LABL": usually before "FMTA"
                // "META": usually after "DATA"
                default:
                    break;
            }

            chunk_offset += 0x18 + chunk_size;
        }

        if (!fmta_offset || !data_offset || !data_size)
            return NULL;
    }


    int loop_flag, channels, channel_layout = 0;
    int num_samples, sample_rate;
    int32_t loop_start, loop_end;
    uint32_t coef_spacing = 0x00, padding = 0x00;
    uint32_t start_offset;
    uint32_t interleave;

    /* parse FMTA */
    if (version < 0x1f) {
        channels        = read_u8(fmta_offset+0x00, sf); // 32b?
        // 0x01: null?
        channel_layout  = read_u8(fmta_offset+0x04, sf);
        // some mono files are marked as L/BL (maybe meant to be layered?)
        if (channels == 1)
            channel_layout = 0;
    }
    else if (version < 0x2e) {
        channels        =  read_u8(fmta_offset+0x00, sf);
        channel_layout  = read_u16(fmta_offset+0x01, sf);
        // 0x01: channel layout?
        // 0x04: extra?
    }
    else {
        channel_layout  = read_u16(fmta_offset+0x00, sf);
        channels        =  read_u8(fmta_offset+0x02, sf);
        // 0x03: flag?
    }
    if (channels == 0)
        return NULL;


    /* parse DATA (interleaved DSP headers + data) */
    uint32_t header_offset = data_offset;
    if (version == 0x0a) {
        size_t align = 0x03; // possibly 32b align
        header_offset += align;
        data_size -= align;
    }

    num_samples = read_s32(header_offset + 0x00, sf);
    sample_rate = read_s32(header_offset + 0x08, sf);
    loop_flag   = read_u16(header_offset + 0x0C, sf);

    if (ras3_offset) {
        // seen in +2ch tracks
        int block_size          = read_s32(ras3_offset + 0x00, sf);
        int block_samples       = read_s32(ras3_offset + 0x08, sf);
        int loop_start_block    = read_s32(ras3_offset + 0x14, sf);
        int loop_start_sample   = read_s32(ras3_offset + 0x18, sf);
        int loop_end_block      = read_s32(ras3_offset + 0x1C, sf);
        int loop_end_sample     = read_s32(ras3_offset + 0x20, sf);
        padding = read_s32(ras3_offset + 0x0C, sf);

        loop_start = loop_start_block * block_samples + loop_start_sample - padding;
        loop_end   = loop_end_block * block_samples + loop_end_sample - padding;
        if ((loop_start || loop_end) && (loop_start < loop_end))
            loop_flag = 1;

        interleave = block_size / channels; // regular interleave
    }
    else {
        uint32_t loop_start_offset  = read_u32(header_offset + 0x10, sf);
        uint32_t loop_end_offset    = read_u32(header_offset + 0x14, sf);
        loop_start = dsp_nibbles_to_samples(loop_start_offset);
        loop_end   = dsp_nibbles_to_samples(loop_end_offset) + 1;
        interleave = data_size / channels; // full interleave
    }

    if (version < 0x1f) {
        coef_spacing = interleave; // 2 separate audio tracks
        start_offset = header_offset + 0x60;
    }
    else {
        coef_spacing = 0x80;
        start_offset = header_offset + coef_spacing * channels;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RFRM;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample   = loop_end;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;
    vgmstream->channel_layout = channel_layout;

    if (ras3_offset) {
        uint32_t padding_bytes = padding / 14 * 8; /* round to frames */
        start_offset += padding_bytes;
        vgmstream->interleave_first_block_size = interleave - padding_bytes;
        vgmstream->interleave_first_skip = padding_bytes;
    }

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs(vgmstream, sf, header_offset + 0x1C, coef_spacing, big_endian);
    dsp_read_hist (vgmstream, sf, header_offset + 0x40, coef_spacing, big_endian);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
