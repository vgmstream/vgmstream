#include "meta.h"
#include "../coding/coding.h"

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
