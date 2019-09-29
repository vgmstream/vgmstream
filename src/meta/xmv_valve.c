#include "meta.h"
#include "../coding/coding.h"

/* .360.WAV, .PS3.WAV - from Valve games running on Source Engine */
/* [The Orange Box (X360), Portal 2 (PS3/X360), Counter-Strike: Global Offensive (PS3/X360)] */
VGMSTREAM* init_vgmstream_xmv_valve(STREAMFILE* streamFile) {
    VGMSTREAM* vgmstream = NULL;
    int32_t loop_start;
    uint32_t start_offset, data_size, sample_rate, num_samples;
    uint16_t /*loop_block, loop_start_skip,*/ loop_end_skip;
    uint8_t format, freq_mode, channels;
    int loop_flag;

    /* checks */
    if (!check_extensions(streamFile, "wav,lwav"))
        goto fail;

    /* check header magic */
    if (read_32bitBE(0x00, streamFile) != 0x58575620) /* "XMV " */
        goto fail;

    /* only version 4 is known */
    if (read_32bitBE(0x04, streamFile) != 0x04)
        goto fail;

    start_offset = read_32bitBE(0x10, streamFile);
    data_size = read_32bitBE(0x14, streamFile);
    num_samples = read_32bitBE(0x18, streamFile);
    loop_start = read_32bitBE(0x1c, streamFile);

    /* XMA only */
  //loop_block = read_16bitBE(0x20, streamFile);
  //loop_start_skip = read_16bitBE(0x22, streamFile);
    loop_end_skip = read_16bitBE(0x24, streamFile);

    format = read_8bit(0x28, streamFile);
    freq_mode = read_8bit(0x2a, streamFile);
    channels = read_8bit(0x2b, streamFile);

    switch (freq_mode) {
        case 0x00: sample_rate = 11025; break;
        case 0x01: sample_rate = 22050; break;
        case 0x02: sample_rate = 44100; break;
        default: goto fail;
    }

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
            ffmpeg_codec_data* ffmpeg_data;
            uint8_t buf[0x100];
            int block_count, block_size;
            size_t bytes;

            block_size = 0x800;
            block_count = data_size / block_size;

            bytes = ffmpeg_make_riff_xma2(buf, 0x100, num_samples, data_size, channels, sample_rate, block_count, block_size);

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf, bytes, start_offset, data_size);
            if (!ffmpeg_data) goto fail;

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->loop_end_sample -= loop_end_skip;

            xma_fix_raw_samples(vgmstream, streamFile, start_offset, data_size, 0, 1, 1);
            break;
        }
#endif
#ifdef VGM_USE_MPEG
        case 0x03: { /* MP3 */
            mpeg_codec_data *mpeg_data;
            coding_t mpeg_coding;

            mpeg_data = init_mpeg(streamFile, start_offset, &mpeg_coding, channels);
            if (!mpeg_data) goto fail;

            vgmstream->codec_data = mpeg_data;
            vgmstream->coding_type = mpeg_coding;
            vgmstream->layout_type = layout_none;

            /* strangely, number of samples is stored incorrectly for MP3, there's PCM size in this field instead */
            vgmstream->num_samples = pcm_bytes_to_samples(num_samples, channels, 16);
            break;
        }
#endif
        case 0x02: /* ADPCM (not actually implemented, was probably supposed to be Microsoft ADPCM) */
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
