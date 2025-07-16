#include "meta.h"
#include "../coding/coding.h"

/* ASTL - found in Dead Rising (PC) */
VGMSTREAM* init_vgmstream_astl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "ASTL"))
        return NULL;
    if (!check_extensions(sf,"ast"))
        return NULL;

    // 04: null
    // 08: 0x201?
    // 0c: version?
    start_offset = read_u32le(0x10,sf);
    // 14: null?
    // 18: null?
    // 1c: null?
    data_size = read_u32le(0x20,sf);
    // 24: -1?
    // 28: -1?
    // 2c: -1?

    // fmt-like
    int format = read_u16le(0x30 + 0x00, sf);
    channels = read_u16le(0x30 + 0x02, sf);
    int sample_rate = read_s32le(0x30 + 0x04, sf);
    // 08: avg bitrate
    // 0a: block size
    // 0e: bps

    loop_flag = 0; // unlike X360 no apparent loop info in the files


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ASTL;
    vgmstream->sample_rate = sample_rate;

    switch(format) {
        case 0x0001:
            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
            break;

#ifdef VGM_USE_FFMPEG
        case 0xFFFE: { // WAVEFORMATEXTENSIBLE
            uint32_t avg_bitrate = read_u32le(0x30 + 0x08, sf);
            uint16_t block_size = read_u16le(0x30 + 0x0c, sf);
            uint32_t channel_layout = read_u32le(0x30 + 0x14, sf);
             // actually a GUID but first 32b matches fmt (0x0162 / STATIC_KSDATAFORMAT_SUBTYPE_WMAUDIO3 only?)
            uint32_t xwma_format = read_u32le(0x30 + 0x18, sf);

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
                uint32_t sample = read_u32le(offset, sf);
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
