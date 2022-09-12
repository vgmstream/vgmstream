#include "meta.h"
#include "../coding/coding.h"

/* .WAV - from Half-Life 2 (Xbox) */
VGMSTREAM* init_vgmstream_xbox_hlwav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t header_size, data_size, start_offset, sample_rate;
    int32_t loop_start;
    uint8_t format, freq_mode, channels;
    int loop_flag;

    /* checks */
    if (!check_extensions(sf, "wav,lwav"))
        goto fail;

    /* check header and size */
    header_size = read_u32le(0x00, sf);
    if (header_size != 0x14)
        goto fail;

    data_size = read_u32le(0x04, sf);
    start_offset = read_u32le(0x08, sf);
    if (data_size != get_streamfile_size(sf) - start_offset)
        goto fail;

    loop_start = read_s32le(0x0c, sf);
    format = read_u8(0x12, sf);
    freq_mode = read_u8(0x13, sf) & 0x0F;
    channels = (read_u8(0x13, sf) >> 4) & 0x0F;

    switch (freq_mode) {
        case 0x00: sample_rate = 11025; break;
        case 0x01: sample_rate = 22050; break;
        case 0x02: sample_rate = 44100; break;
        default: goto fail;
    }

    if (channels > 2) /* Source only supports mono and stereo */
        goto fail;

    loop_flag = (loop_start != -1);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_XBOX_HLWAV;
    vgmstream->sample_rate = sample_rate;
    vgmstream->loop_start_sample = loop_start;

    switch (format) {
        case 0x00: /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            vgmstream->loop_end_sample = vgmstream->num_samples; /* always loops from the end */
            break;
        case 0x01: /* XBOX ADPCM */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;
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

/* .360.WAV, .PS3.WAV - from Valve games running on Source Engine, evolution of Xbox .WAV format seen above */
/* [The Orange Box (X360), Portal 2 (PS3/X360), Counter-Strike: Global Offensive (PS3/X360)] */
VGMSTREAM* init_vgmstream_xmv_valve(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int32_t loop_start;
    uint32_t start_offset, data_size, sample_rate, num_samples;
    uint16_t /*loop_block, loop_start_skip,*/ loop_end_skip;
    uint8_t format, freq_mode, channels;
    int loop_flag;

    /* checks */
    if (!is_id32be(0x00, sf, "XMV "))
        goto fail;

    if (!check_extensions(sf, "wav,lwav"))
        goto fail;

    /* only version 4 is known */
    if (read_u32be(0x04, sf) != 0x04)
        goto fail;

    start_offset = read_u32be(0x10, sf);
    data_size = read_u32be(0x14, sf);
    num_samples = read_u32be(0x18, sf);
    loop_start = read_s32be(0x1c, sf);

    /* XMA only */
  //loop_block = read_u16be(0x20, sf);
  //loop_start_skip = read_u16be(0x22, sf);
    loop_end_skip = read_u16be(0x24, sf);

    format = read_u8(0x28, sf);
    freq_mode = read_u8(0x2a, sf);
    channels = read_u8(0x2b, sf);

    switch (freq_mode) {
        case 0x00: sample_rate = 11025; break;
        case 0x01: sample_rate = 22050; break;
        case 0x02: sample_rate = 44100; break;
        default: goto fail;
    }

    if (channels > 2) /* Source only supports mono and stereo */
        goto fail;

    loop_flag = (loop_start != -1);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMV_VALVE;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples; /* always loops from the end */

    switch (format) {
        case 0x00: /* PCM */
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;
#ifdef VGM_USE_FFMPEG
        case 0x01: { /* XMA */
            uint8_t buf[0x100];
            int block_count, block_size;
            size_t bytes;

            block_size = 0x800;
            block_count = data_size / block_size;

            bytes = ffmpeg_make_riff_xma2(buf, 0x100, num_samples, data_size, channels, sample_rate, block_count, block_size);

            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf, bytes, start_offset, data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->loop_end_sample -= loop_end_skip;

            xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, 0, 1, 1);
            break;
        }
#endif
#ifdef VGM_USE_MPEG
        case 0x03: { /* MP3 */
            coding_t mpeg_coding;

            if (loop_flag) /* should never happen, Source cannot loop MP3 */
                goto fail;

            vgmstream->codec_data = init_mpeg(sf, start_offset, &mpeg_coding, channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = mpeg_coding;
            vgmstream->layout_type = layout_none;

            /* strangely, number of samples is stored incorrectly for MP3, there's PCM size in this field instead */
            vgmstream->num_samples = pcm_bytes_to_samples(num_samples, channels, 16);
            break;
        }
#endif
        case 0x02: /* ADPCM (not actually implemented, was probably supposed to be Microsoft ADPCM or Xbox IMA) */
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
