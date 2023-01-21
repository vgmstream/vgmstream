#include "meta.h"
#include "../coding/coding.h"

/* SEG - from Stormfront games [Eragon (multi), Forgotten Realms: Demon Stone (multi) */
VGMSTREAM* init_vgmstream_seg(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;
    uint32_t codec;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "seg\0"))
        goto fail;
    if (!check_extensions(sf, "seg"))
        goto fail;

    codec = read_32bitBE(0x04,sf);
    /* 0x08: version? (2: Eragon, Spiderwick Chronicles Wii / 3: Spiderwick Chronicles X360 / 4: Spiderwick Chronicles PC) */
    if (guess_endianness32bit(0x08,sf)) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }
    /* 0x0c: file size */
    data_size = read_32bit(0x10, sf); /* including interleave padding */
    /* 0x14: null */

    loop_flag = read_32bit(0x20,sf); /* rare */
    channel_count = read_32bit(0x24,sf);
    /* 0x28: extradata 1 entries (0x08 per entry, unknown) */
    /* 0x2c: extradata 1 offset */
    /* 0x30: extradata 2 entries (0x10 or 0x14 per entry, seek/hist table?) */
    /* 0x34: extradata 2 offset */

    start_offset = 0x4000;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SEG;
    vgmstream->sample_rate = read_32bit(0x18,sf);
    vgmstream->num_samples = read_32bit(0x1c,sf);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }
    read_string(vgmstream->stream_name,0x20+1, 0x38,sf);

    switch(codec) {
        case 0x70733200: /* "ps2\0" */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2000;
            break;

        case 0x78627800: /* "xbx\0" */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case 0x77696900: /* "wii\0" */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2000;
            vgmstream->interleave_first_skip = 0x60;
            vgmstream->interleave_first_block_size = vgmstream->interleave_block_size - vgmstream->interleave_first_skip;

            /* standard dsp header at start_offset */
            dsp_read_coefs_be(vgmstream, sf, start_offset+0x1c, vgmstream->interleave_block_size);
            dsp_read_hist_be(vgmstream, sf, start_offset+0x40, vgmstream->interleave_block_size);

            start_offset += vgmstream->interleave_first_skip;
            break;

        case 0x70635F00: /* "pc_\0" */
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_none;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x78623300: { /* "xb3\0" */
            int block_size = 0x4000;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, start_offset, data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, start_offset,data_size, 0, 0,0); /* samples are ok */
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
