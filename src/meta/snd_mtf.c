#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* SNDB/SNDL - from early MT Framework games [Dead Rising (PC/X360), Lost Planet (X360)] */
VGMSTREAM* init_vgmstream_snd_mtf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t csb_offset, stream_offset, entry_offset;
    size_t stream_size;
    int loop_flag, channels, sample_rate, format, version;
    int total_subsongs, target_subsong = sf->stream_index;
    read_u32_t read_u32;
    read_s32_t read_s32;
    read_u16_t read_u16;


    /* checks */
    if (is_id32be(0x00, sf, "SNDB")) {
        read_u16 = read_u16be;
        read_s32 = read_s32be;
        read_u32 = read_u32be;
    }
    else if (is_id32be(0x00, sf, "SNDL")) {
        read_u16 = read_u16le;
        read_s32 = read_s32le;
        read_u32 = read_u32le;
    }
    else {
        return NULL;
    }

    /* .snd: early/dev builds
     * (extensionless): names in bigfiles
     * .rSoundSnd: fake/added by tools */
    if (!check_extensions(sf, "snd,,rsoundsnd"))
        return NULL;


    // 04: file size
    version    = read_u32(0x08, sf);
    // 0c: -1?
    // 10: CSR offset (0x20)
    // 14: CSH offset
    csb_offset = read_u32(0x18, sf);
    // 1c: -1?

    // v2.0: Dead Rising, v3.0: Lost Planet
    // not == because v2.1 is seen in ast_mtf
    if (version < 0x0200 && version > 0x0300)
        goto fail;

    if (!is_id32be(csb_offset + 0x00, sf, "CSB "))
        goto fail;
    // 04: csb size
    total_subsongs = read_u32(csb_offset + 0x08, sf);
    entry_offset   = read_u32(csb_offset + 0x0c, sf) + csb_offset; // 0x20
    stream_offset  = read_u32(csb_offset + 0x10, sf) + csb_offset; // usually aligned to 0x800
    // 14: sound data size
    // 18: format (also repeated for each entry)
    // 1c: -1?

    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;
    if (target_subsong == 0) target_subsong = 1;
    target_subsong--; // zero indexing


    entry_offset += target_subsong * ((version < 0x0300) ? 0x40 : 0x70);

    stream_size     = read_u32(entry_offset + 0x00, sf);
    stream_offset  += read_u32(entry_offset + 0x04, sf);
    // 08: ? (some small value between 1-15)
    // 0c: -1 x5?
    format          = read_u16(entry_offset + 0x20, sf);
    // fmt-like
    if (format == 0x0165) { // XMA1/2
        // 22: xma info size
        // 24: xma config
        int streams = read_u16(entry_offset + 0x28, sf);
        loop_flag   = read_u8 (entry_offset + 0x2a, sf);
        sample_rate = read_s32(entry_offset + 0x30, sf);
        channels    = read_u8 (entry_offset + 0x3d, sf);

        if (streams != 1) {
            vgm_logi("SNDB: multi-stream XMA found (report)\n");
            goto fail;
        }
        /*
        channels = 0; // sum of all stream channels like ast_mtf?
        for (int i = 0; i < xma_streams; i++) {
            channels += read_u8(0x3c + 0x14 * i + 0x11, sf);
        }
        */
        if (version >= 0x0300) {
            // 40: xma2 fmt
            // 64: -1 x3?
            if (read_u8 (entry_offset + 0x41, sf) != streams ||
                read_s32(entry_offset + 0x4c, sf) != sample_rate ||
                read_u8 (entry_offset + 0x60, sf) != channels) {
                VGM_LOG("SNDB: XMA2 data mismatch\n");
                goto fail;
            }
        }
    }
    else { // PCM (and xWMA in ast_mtf?)
        channels    = read_u16(entry_offset + 0x22, sf);
        sample_rate = read_s32(entry_offset + 0x24, sf);
        // 28: avg bitrate
        // 2a: block size
        // 2e: bps
        // 30: -1 x4?

        loop_flag = 0; // unlike X360 no apparent loop info in the files
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SND_MTF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;

    switch (format) {
        case 0x0001: // PCM
            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0165: { // XMA1/2
            off_t fmt_offset;
            size_t fmt_size;

            if (version < 0x0300) {
                /* manually find sample offsets (XMA1 nonsense again) */
                ms_sample_data msd = {0};

                msd.xma_version = 1;
                msd.channels = channels;
                msd.data_offset = stream_offset;
                msd.data_size = stream_size;
                msd.loop_flag = loop_flag;
                msd.loop_start_b = read_u32(entry_offset + 0x34, sf);
                msd.loop_end_b   = read_u32(entry_offset + 0x38, sf);
                msd.loop_start_subframe = read_u8(entry_offset + 0x3c, sf) & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
                msd.loop_end_subframe   = read_u8(entry_offset + 0x3c, sf) >> 4;  /* upper 4b: subframe where the loop ends, 0..3 */

                xma_get_samples(&msd, sf);
                vgmstream->num_samples = msd.num_samples;
                vgmstream->loop_start_sample = msd.loop_start_sample;
                vgmstream->loop_end_sample = msd.loop_end_sample;

                fmt_offset = entry_offset + 0x20;
                fmt_size = 0x20;
            }
            else {
                vgmstream->loop_start_sample = read_u32(entry_offset + 0x44, sf);
                vgmstream->loop_end_sample   = read_u32(entry_offset + 0x48, sf);
                vgmstream->num_samples       = read_u32(entry_offset + 0x58, sf);

                fmt_offset = entry_offset + 0x40;
                fmt_size = 0x24;
            }

            /* XMA "fmt" chunk (BE, unlike the usual LE) */
            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, stream_offset, stream_size, fmt_offset, fmt_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, stream_offset, stream_size, fmt_offset, 1, 1);
            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
