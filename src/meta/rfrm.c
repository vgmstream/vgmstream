#include "meta.h"
#include "../coding/coding.h"

/* RFTM - Retro Studios format [Metroid Prime Remastered] */
VGMSTREAM *init_vgmstream_rfrm_mpr(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    off_t fmta_offset = 0, data_offset = 0, ras3_offset = 0, header_offset, start_offset;
    size_t data_size = 0, interleave;
    int loop_flag, channel_count;
    int loop_start, loop_end, padding;

    /* checks */
    if (!check_extensions(streamFile, "csmp"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x5246524D) /* "RFRM" */
        goto fail;
    /* 0x08: file size but not exact */
    if (read_32bitBE(0x14, streamFile) != 0x43534D50) /* "CSMP" */
        goto fail;

    if (read_32bitLE(0x18,streamFile) != 0x1F) /* assumed, also at 0x1c */
        goto fail;


    /* parse chunks (always BE) */
    {
        off_t chunk_offset = 0x20;
        off_t file_size = get_streamfile_size(streamFile);

        while (chunk_offset < file_size) {
            uint32_t chunk_type = read_32bitBE(chunk_offset + 0x00,streamFile);
            size_t chunk_size   = read_32bitLE(chunk_offset + 0x08,streamFile);

            switch(chunk_type) {
                case 0x464D5441: /* "FMTA" */
                    fmta_offset = chunk_offset + 0x18;
                    chunk_offset += 5 + 0x18 + chunk_size;
                    break;
                case 0x44415441: /* "DATA" */
                    data_offset = chunk_offset + 0x18;
                    data_size = read_32bitLE(chunk_offset + 0x04, streamFile);
                    /* we're done here, DATA is the last chunk */
                    chunk_offset = file_size;
                    break;
                case 0x52415333: /* "RAS3" */
                    ras3_offset = chunk_offset + 0x18;
                    chunk_offset += 60;
                    break;
                case 0x43524D53: /* CRMS */
                    chunk_offset += 9 + 0x18 + chunk_size + read_32bitLE(chunk_offset + 0x18 + chunk_size + 5, streamFile);
                    break;
                default:
                    goto fail;
            }
        }

        if (!fmta_offset || !data_offset || !data_size)
            goto fail;
    }


    /* parse FMTA / DATA (fully interleaved standard DSPs) */
    channel_count = read_8bit(fmta_offset + 0x00, streamFile);
    /* FMTA 0x08: channel mapping */

    header_offset = data_offset;
    start_offset = header_offset + 0x80 * channel_count;
    loop_flag = read_16bitLE(header_offset + 0x0C, streamFile);
    interleave = data_size / channel_count;

    if (ras3_offset) {
        int block_size = read_32bitLE(ras3_offset + 0x00, streamFile);
        int block_samples = read_32bitLE(ras3_offset + 0x8, streamFile);
        int loop_start_block = read_32bitLE(ras3_offset + 0x14, streamFile);
        int loop_start_sample = read_32bitLE(ras3_offset + 0x18, streamFile);
        int loop_end_block = read_32bitLE(ras3_offset + 0x1C, streamFile);
        int loop_end_sample = read_32bitLE(ras3_offset + 0x20, streamFile);
        padding = read_32bitLE(ras3_offset + 0x0C, streamFile);

        loop_start = loop_start_block * block_samples + loop_start_sample - padding;
        loop_end   = loop_end_block * block_samples + loop_end_sample - padding;
        if ((loop_start || loop_end) && (loop_start < loop_end))
            loop_flag = 1;

        interleave = block_size / channel_count;
    } else {
        loop_start = dsp_nibbles_to_samples(read_32bitLE(header_offset + 0x10, streamFile));
        loop_end   = dsp_nibbles_to_samples(read_32bitLE(header_offset + 0x14, streamFile)) + 1;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RFRM;
    vgmstream->sample_rate = read_32bitLE(header_offset + 0x08, streamFile);
    vgmstream->num_samples = read_32bitLE(header_offset + 0x00, streamFile);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample   = loop_end;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    if (ras3_offset) {
        int padding_bytes = padding / 14 * 8; /* round to frames */
        vgmstream->interleave_first_block_size = interleave - padding_bytes;
        vgmstream->interleave_first_skip = padding_bytes;
        start_offset += padding_bytes;
    }

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs(vgmstream, streamFile, header_offset + 0x1C, 0x80, 0);
    dsp_read_hist (vgmstream, streamFile, header_offset + 0x40, 0x80, 0);

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* RFTM - Retro Studios format [Donkey Kong Country Tropical Freeze (WiiU/Switch)] */
VGMSTREAM *init_vgmstream_rfrm(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    off_t fmta_offset = 0, data_offset = 0, header_offset, start_offset;
    size_t data_size = 0, interleave;
    int loop_flag, channel_count, version;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* checks */
    if (!check_extensions(streamFile, "csmp"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x5246524D) /* "RFRM" */
        goto fail;
    /* 0x08: file size but not exact */
    if (read_32bitBE(0x14, streamFile) != 0x43534D50) /* "CSMP" */
        goto fail;
    version = read_32bitBE(0x18,streamFile); /* assumed, also at 0x1c */

    if (version == 0x0a) { /* Wii U */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    }
    else if (version == 0x12) { /* Switch */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        big_endian = 0;
    }
    else if (version == 0x1F000000) { /* Metroid Prime Remastered */
        return init_vgmstream_rfrm_mpr(streamFile);
    }
    else {
        goto fail;
    }


    /* parse chunks (always BE) */
    {
        off_t chunk_offset = 0x20;
        off_t file_size = get_streamfile_size(streamFile);

        while (chunk_offset < file_size) {
            uint32_t chunk_type = read_32bitBE(chunk_offset+0x00,streamFile);
            size_t chunk_size   = read_32bitBE(chunk_offset+0x08,streamFile); /* maybe 64b from 0x04? */

            switch(chunk_type) {
                case 0x464D5441: /* "FMTA" */
                    fmta_offset = chunk_offset + 0x18;
                    break;
                case 0x44415441: /* "DATA" */
                    data_offset = chunk_offset + 0x18;
                    data_size = chunk_size;
                    break;
                default: /* known: "LABL" (usually before "FMTA"), "META" (usually after "DATA") */
                    break;
            }

            chunk_offset += 0x18 + chunk_size;
        }

        if (!fmta_offset || !data_offset || !data_size)
            goto fail;
    }


    /* parse FMTA / DATA (fully interleaved standard DSPs) */
    channel_count = read_8bit(fmta_offset+0x00, streamFile);
    /* FMTA 0x08: channel mapping */

    header_offset = data_offset;
    if (version == 0x0a) {
        size_t align = 0x03; /* possibly 32b align */
        header_offset += align;
        data_size -= align;
    }
    start_offset = header_offset + 0x60;
    loop_flag = read_16bit(header_offset + 0x0C, streamFile);
    interleave = data_size / channel_count;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RFRM;
    vgmstream->sample_rate = read_32bit(header_offset + 0x08, streamFile);
    vgmstream->num_samples = read_32bit(header_offset + 0x00, streamFile);
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bit(header_offset + 0x10, streamFile));
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(read_32bit(header_offset + 0x14, streamFile)) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs(vgmstream, streamFile, header_offset + 0x1C, interleave, big_endian);
    dsp_read_hist (vgmstream, streamFile, header_offset + 0x40, interleave, big_endian);

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
