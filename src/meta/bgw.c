#include "meta.h"
#include "../coding/coding.h"
#include "bgw_streamfile.h"


/* BGW - from Final Fantasy XI (PC) music files */
VGMSTREAM* init_vgmstream_bgw(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t codec, file_size, block_size, sample_rate, block_align;
    int32_t loop_start;
    off_t start_offset;

    int channels, loop_flag = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "BGMS") || !is_id32be(0x04,sf, "trea") || !is_id32be(0x08,sf, "m\0\0\0"))
        return NULL;

    if (!check_extensions(sf, "bgw"))
        return NULL;

    codec = read_u32le(0x0c,sf);
    file_size = read_u32le(0x10,sf);
    //14: file_id
    block_size = read_u32le(0x18,sf);
    loop_start = read_s32le(0x1c,sf);
    sample_rate = (read_u32le(0x20,sf) + read_u32le(0x24,sf)) & 0x7FFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_u32le(0x28,sf);
    //2c: unk (vol?)
    //2d: unk (bps?)
    channels = read_s8(0x2e,sf);
    block_align = read_u8(0x2f,sf);

    if (file_size != get_streamfile_size(sf))
        goto fail;

    loop_flag = (loop_start > 0);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BGW;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0: /* PS ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */

            vgmstream->num_samples = block_size * block_align;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1) * block_align;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;

#ifdef VGM_USE_FFMPEG
        case 3: { /* ATRAC3 (encrypted) */
            size_t data_size = file_size - start_offset;
            int encoder_delay, frame_size;

            encoder_delay = 1024*2 + 69*2; /* observed value, all files start at +2200 (PS-ADPCM also starts around 50-150 samples in) */
            frame_size = 0xC0 * vgmstream->channels; /* 0x00 in header */
            vgmstream->num_samples = block_size - encoder_delay; /* atrac3_bytes_to_samples gives block_size */

            temp_sf = setup_bgw_atrac3_streamfile(sf, start_offset,data_size, 0xC0,channels);
            if (!temp_sf) goto fail;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(temp_sf, 0x00, data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, frame_size, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start - encoder_delay;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            close_streamfile(temp_sf);
            temp_sf = NULL;
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
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


/* SPW (SEWave) - from PlayOnline viewer for Final Fantasy XI (PC) */
VGMSTREAM* init_vgmstream_spw(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t codec, file_size, block_size, sample_rate, block_align;
    int32_t loop_start;
    off_t start_offset;

    int channels, loop_flag = 0;

    /* checks */
    if (!is_id32be(0x00,sf, "SeWa") || !is_id32be(0x04,sf, "ve\0\0"))
        return NULL;

    if (!check_extensions(sf, "spw"))
        return NULL;

    file_size = read_u32le(0x08,sf);
    codec = read_u32le(0x0c,sf);
    //10: file_id
    block_size = read_u32le(0x14,sf);
    loop_start = read_s32le(0x18,sf);
    sample_rate = (read_u32le(0x1c,sf) + read_u32le(0x20,sf)) & 0x7FFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_u32le(0x24,sf);
    // 2c: unk (0x00?)
    // 2d: unk (0x00/01?)
    channels = read_s8(0x2a,sf);
    /*0x2b: unk (0x01 when PCM, 0x10 when VAG?) */
    block_align = read_u8(0x2c,sf);

    if (file_size != get_streamfile_size(sf))
        goto fail;

    loop_flag = (loop_start > 0);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SPW;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0: /* PS ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */
            
            vgmstream->num_samples = block_size * block_align;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1) * block_align;;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;

        case 1: /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            
            vgmstream->num_samples = block_size;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1);
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            break;

#ifdef VGM_USE_FFMPEG
        case 3: { /* ATRAC3 (encrypted) */
            size_t data_size = file_size - start_offset;
            int encoder_delay, frame_size;

            encoder_delay = 1024*2 + 69*2; /* observed value, all files start at +2200 (PS-ADPCM also starts around 50-150 samples in) */
            frame_size = 0xC0 * vgmstream->channels; /* 0x00 in header */
            vgmstream->num_samples = block_size - encoder_delay; /* atrac3_bytes_to_samples gives block_size */

            temp_sf = setup_bgw_atrac3_streamfile(sf, start_offset,data_size, 0xC0, channels);
            if (!temp_sf) goto fail;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(temp_sf, 0x00, data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, frame_size, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start - encoder_delay;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            close_streamfile(temp_sf);
            temp_sf = NULL;
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
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
