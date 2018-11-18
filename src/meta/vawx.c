#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* VAWX - found in feelplus games [No More Heroes: Heroes Paradise (PS3/X360), Moon Diver (PS3/X360)] */
VGMSTREAM * init_vgmstream_vawx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag = 0, channel_count, codec;


    /* checks */
    /* .xwv: actual extension [Moon Diver (PS3/X360)]
     * .vawx: header id */
    if ( !check_extensions(streamFile, "xwv,vawx") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x56415758) /* "VAWX" */
        goto fail;

    loop_flag = read_8bit(0x37,streamFile);
    channel_count = read_8bit(0x39,streamFile);
    start_offset = 0x800; /* ? read_32bitLE(0x0c,streamFile); */
    codec = read_8bit(0x36,streamFile); /* could be at 0x38 too */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x04: filesize */
    /* 0x16: file id */
    vgmstream->num_samples = read_32bitBE(0x3c,streamFile);
    vgmstream->sample_rate = read_32bitBE(0x40,streamFile);

    vgmstream->meta_type = meta_VAWX;

    switch(codec) {
        case 2: /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = channel_count == 6 ? layout_blocked_vawx : layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->loop_start_sample = read_32bitBE(0x44,streamFile);
            vgmstream->loop_end_sample = read_32bitBE(0x48,streamFile);

            break;

#ifdef VGM_USE_FFMPEG
        case 1: { /* XMA2 */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[0x100];
            int32_t bytes, block_size, block_count;

            data_size = get_streamfile_size(streamFile)-start_offset;
            block_size = 0x10000; /* VAWX default */
            block_count = (uint16_t)read_16bitBE(0x3A, streamFile); /* also at 0x56 */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = read_32bitBE(0x44,streamFile);
            vgmstream->loop_end_sample = read_32bitBE(0x48,streamFile);

            /* may be only applying end_skip to num_samples? */
            xma_fix_raw_samples(vgmstream, streamFile, start_offset,data_size, 0, 0,0);
            break;
        }

        case 7: { /* ATRAC3 */
            uint8_t buf[0x100];
            int32_t bytes, block_size, encoder_delay, joint_stereo, max_samples;

            data_size = read_32bitBE(0x54,streamFile);
            block_size = 0x98 * vgmstream->channels;
            joint_stereo = 0;
            max_samples = atrac3_bytes_to_samples(data_size, block_size);
            encoder_delay = 0x0; //max_samples - vgmstream->num_samples; /* todo not correct */
            vgmstream->num_samples = max_samples; /* use calc samples since loop points are too, breaks looping in some files otherwise */

            /* make a fake riff so FFmpeg can parse the ATRAC3 */
            bytes = ffmpeg_make_riff_atrac3(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = atrac3_bytes_to_samples(read_32bitBE(0x44,streamFile), block_size);
            vgmstream->loop_end_sample   = atrac3_bytes_to_samples(read_32bitBE(0x48,streamFile), block_size);
            //vgmstream->loop_start_sample -= encoder_delay;
            //vgmstream->loop_end_sample   -= encoder_delay;

            break;
        }
#endif
        default:
            goto fail;

    }


    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
