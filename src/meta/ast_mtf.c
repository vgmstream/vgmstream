#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* ASTB/ASTL - from early MT Framework games [Dead Rising (PC/X360), Lost Planet (X360)] */
VGMSTREAM* init_vgmstream_ast_mtf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size, fmt_offset;
    int loop_flag, channels, format, version;
    int sample_rate, xma_streams;
    read_u32_t read_u32;
    read_s32_t read_s32;
    read_u16_t read_u16;

    /* checks */
    if (is_id32be(0x00, sf, "ASTB")) {
        read_u16 = read_u16be;
        read_s32 = read_s32be;
        read_u32 = read_u32be;
    }
    else if (is_id32be(0x00, sf, "ASTL")) {
        read_u16 = read_u16le;
        read_s32 = read_s32le;
        read_u32 = read_u32le;
    }
    else {
        return NULL;
    }

    /* .ast: early/dev builds
     * (extensionless): names in .arc
     * .rSoundAst: fake/added by tools */
    if (!check_extensions(sf, "ast,,rsoundast"))
        return NULL;


    // 04: file size
    version      = read_u32(0x08, sf);
    // 0c: sub-version?
    start_offset = read_u32(0x10, sf);
    // 14: -1?
    // 18: -1?
    // 1c: -1?
    data_size    = read_u32(0x20, sf);
    // 24: -1?
    // 28: -1?
    // 2c: -1?

    /* v2.0: Dead Rising (X360)
     * v2.1: Dead Rising (PC)
     * v3.0: Lost Planet (X360) */
    if (version < 0x0200 && version > 0x0300)
        goto fail;

    // fmt-like
    fmt_offset = 0x30;
    format = read_u16(fmt_offset + 0x00, sf);

    if (format == 0x0165) { // XMA1/2
        // 32: xma info size
        // 34: xma config
        xma_streams = read_u16(fmt_offset + 0x08, sf);
        loop_flag   = read_u8 (fmt_offset + 0x0a, sf);

        sample_rate = read_s32(fmt_offset + 0x0c + 0x04, sf); // first stream
        channels = 0; // sum of all stream channels (though only 1/2ch are ever seen)
        for (int i = 0; i < xma_streams; i++) {
            channels += read_u8(fmt_offset + 0x0c + 0x14 * i + 0x11, sf);
        }

        if (version >= 0x0300) {
            fmt_offset += 0x0c + xma_streams * 0x14;

            // xma2 fmt config
            int temp_channels = 0;
            for (int i = 0; i < xma_streams; i++) {
                temp_channels += read_u8(fmt_offset + 0x20 + 0x04 * i + 0x00, sf);
            }

            if (read_u8 (fmt_offset + 0x01, sf) != xma_streams ||
                read_s32(fmt_offset + 0x0c, sf) != sample_rate ||
                channels != temp_channels) {
                VGM_LOG("ASTB: XMA2 data mismatch\n");
                goto fail;
            }
        }
    }
    else { // PCM/xWMA
        channels    = read_u16(0x30 + 0x02, sf);
        sample_rate = read_s32(0x30 + 0x04, sf);
        // 08: avg bitrate
        // 0a: block size
        // 0e: bps

        loop_flag = 0; // unlike X360 no apparent loop info in the files
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AST_MTF;
    vgmstream->sample_rate = sample_rate;


    switch (format) {
        case 0x0001: // PCM
            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0165: { // XMA1/2
            size_t fmt_size;

            if (version < 0x0300) {
                /* manually find sample offsets (XMA1 nonsense again) */
                ms_sample_data msd = {0};

                msd.xma_version = 1;
                msd.channels = channels;
                msd.data_offset = start_offset;
                msd.data_size = data_size;
                msd.loop_flag = loop_flag;
                msd.loop_start_b = read_u32(fmt_offset + 0x14, sf);
                msd.loop_end_b   = read_u32(fmt_offset + 0x18, sf);
                msd.loop_start_subframe = read_u8(fmt_offset + 0x1c, sf) & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
                msd.loop_end_subframe   = read_u8(fmt_offset + 0x1c, sf) >> 4;  /* upper 4b: subframe where the loop ends, 0..3 */

                xma_get_samples(&msd, sf);
                vgmstream->num_samples = msd.num_samples;
                vgmstream->loop_start_sample = msd.loop_start_sample;
                vgmstream->loop_end_sample = msd.loop_end_sample;

                fmt_size = 0x0c + xma_streams * 0x14;
            }
            else {
                vgmstream->loop_start_sample = read_u32(fmt_offset + 0x04, sf);
                vgmstream->loop_end_sample   = read_u32(fmt_offset + 0x08, sf);
                vgmstream->num_samples       = read_u32(fmt_offset + 0x18, sf);

                fmt_size = 0x20 + xma_streams * 0x04;
            }

            /* XMA "fmt" chunk (BE, unlike the usual LE) */
            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, fmt_offset, fmt_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, fmt_offset, 1, 1);
            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case 0xFFFE: { // WAVEFORMATEXTENSIBLE
            uint32_t avg_bitrate    = read_u32(0x30 + 0x08, sf);
            uint16_t block_size     = read_u16(0x30 + 0x0c, sf);
            uint32_t channel_layout = read_u32(0x30 + 0x14, sf);
            // actually a GUID but first 32b matches fmt (0x0162 / STATIC_KSDATAFORMAT_SUBTYPE_WMAUDIO3 only?)
            uint32_t xwma_format    = read_u32(0x30 + 0x18, sf);

            uint32_t dpds_offset = 0x30 + 0x28;
            uint32_t dpds_size;
            uint32_t offset = dpds_offset;
            // TODO channel layout

            vgmstream->codec_data = init_ffmpeg_xwma(sf, start_offset, data_size, xwma_format, channels, sample_rate, avg_bitrate, block_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->channel_layout = channel_layout;

            // somehow there is no dpds size, so find last value before null padding
            while (offset < start_offset) {
                uint32_t sample = read_u32(offset, sf);
                if (sample == 0)
                    break;
                offset += 0x04;
            }

            dpds_size = offset - dpds_offset;
            vgmstream->num_samples = xwma_dpds_get_samples(sf, dpds_offset, dpds_size, channels, false);
            break;
        }
#endif
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
