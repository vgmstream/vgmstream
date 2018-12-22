#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

typedef enum { NONE, PSX, DSP, XBOX } mul_codec;

/* .MUL - from Crystal Dynamics games [Legacy of Kain: Defiance (PS2), Tomb Raider Underworld (multi)] */
VGMSTREAM * init_vgmstream_mul(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, coefs_offset = 0;
    int loop_flag, channel_count, sample_rate, num_samples, loop_start;
    int big_endian;
    mul_codec codec = NONE;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .mul: found in the exe, used by the bigfile extractor (Gibbed.TombRaider)
     *       (some files have companion .mus/sam files but seem to be sequences/control stuff)
     * .(extensionless): filenames as found in the bigfile
     * .emff: fake extension ('Eidos Music File Format') */
    if (!check_extensions(streamFile, "mul,,emff"))
        goto fail;
    if (read_32bitBE(0x10,streamFile) != 0 ||
        read_32bitBE(0x14,streamFile) != 0 ||
        read_32bitBE(0x18,streamFile) != 0 ||
        read_32bitBE(0x1c,streamFile) != 0)
        goto fail;

    big_endian = guess_endianness32bit(0x00, streamFile);
    read_32bit = big_endian ? read_32bitBE : read_32bitLE;

    sample_rate   = read_32bit(0x00,streamFile);
    loop_start    = read_32bit(0x04,streamFile);
    num_samples   = read_32bit(0x08,streamFile);
    channel_count = read_32bit(0x0C,streamFile);
    if (sample_rate < 8000 || sample_rate > 48000 || channel_count > 8)
        goto fail;
    /* 0x20: flag when file has non-audio blocks (ignored by the layout) */
    /* 0x24: same? */
    /* 0x28: loop offset within audio data (not file offset) */
    /* 0x2c: some value related to loop? */
    /* 0x34: id? */
    /* 0x38+: channel config until ~0x100? (multiple 0x3F800000 depending on the number of channels) */

    /* test known versions (later versions start from 0x24 instead of 0x20) */
    if (!(read_32bit(0x38,streamFile) == 0x3F800000 ||
          read_32bit(0x3c,streamFile) == 0x3F800000))   /* Tomb Raider Underworld */
        goto fail;

    loop_flag = (loop_start >= 0); /* 0xFFFFFFFF when not looping */
    start_offset = 0x800;

    /* format is pretty limited so we need to guess codec */
    if (big_endian) {
        /* test DSP (GC/Wii): check known coef locations */
        if (read_32bitBE(0xC8,streamFile) != 0) { /*  Tomb Raider Legend (GC) */
            codec = DSP;
            coefs_offset = 0xC8;
        }
        else if (read_32bitBE(0xCC,streamFile) != 0) { /* Tomb Raider Anniversary (Wii) */
            codec = DSP;
            coefs_offset = 0xCC;
        }
        else if (read_32bitBE(0x2D0,streamFile) != 0) { /* Tomb Raider Underworld (Wii) */
            codec = DSP;
            coefs_offset = 0x2D0;
        }

        // todo test XMA1 (X360): mono streams, each block has 1 sub-blocks of 0x800 packet per channel

        // todo test ? (PS3)
    }
    else {
        int i;
        off_t offset = start_offset;
        size_t file_size = get_streamfile_size(streamFile);
        size_t frame_size;

        /* check first audio frame */
        while (offset < file_size) {
            uint32_t block_type = read_32bit(offset+0x00, streamFile);
            uint32_t block_size = read_32bit(offset+0x04, streamFile);
            uint32_t data_size  = read_32bit(offset+0x10, streamFile);

            if (block_type != 0x00) {
                offset += 0x10 + block_size;
                continue; /* not audio */
            }

            /* test PS-ADPCM (PS2/PSP): flag is always 2 in .mul */
            frame_size = 0x10;
            for (i = 0; i < data_size / frame_size; i++) {
                if (read_8bit(offset + 0x20 + frame_size*i + 0x01, streamFile) != 0x02)
                    break;
            }
            if (i == data_size / frame_size) {
                codec = PSX;
                break;
            }

            /* test XBOX-IMA (PC/Xbox): reserved frame header value is always 0 */
            frame_size = 0x24;
            for (i = 0; i < data_size / frame_size; i++) {
                if (read_8bit(offset + 0x20 + frame_size*i + 0x03, streamFile) != 0x00)
                    break;
            }
            if (i == data_size / frame_size) {
                codec = XBOX;
                break;
            }

            break;
        }
    }

    if (codec == NONE) {
        VGM_LOG("MUL: unknown codec\n");
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MUL;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;
    vgmstream->codec_endian = big_endian;

    switch(codec) {
        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_blocked_mul;
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_mul;

            dsp_read_coefs_be(vgmstream,streamFile,coefs_offset+0x00,0x2e);
            dsp_read_hist_be (vgmstream,streamFile,coefs_offset+0x24,0x2e);
            break;

        case XBOX:
            vgmstream->coding_type = coding_XBOX_IMA_int;
            vgmstream->layout_type = layout_blocked_mul;
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
