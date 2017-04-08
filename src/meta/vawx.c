#include "meta.h"
#include "../coding/coding.h"

#define FAKE_RIFF_BUFFER_SIZE           100

/**
 * VAWX - found in feelplus games: No More Heroes Heroes Paradise, Moon Diver
 */
VGMSTREAM * init_vgmstream_vawx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, datasize;

    int loop_flag = 0, channel_count, type;

    /* check extensions */
    if ( !check_extensions(streamFile, "vawx,xwv") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x56415758) /* "VAWX" */
        goto fail;

    loop_flag = read_8bit(0x37,streamFile);
    channel_count = read_8bit(0x39,streamFile);;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x04: filesize */
    start_offset = 0x800; /* ? read_32bitLE(0x0c,streamFile); */
    vgmstream->channels = channel_count;
    /* 0x16: file id */
    type = read_8bit(0x36,streamFile); /* could be at 0x38 too */
    vgmstream->num_samples = read_32bitBE(0x3c,streamFile);
    vgmstream->sample_rate = read_32bitBE(0x40,streamFile);

    vgmstream->meta_type = meta_VAWX;

    switch(type) {
        case 2: /* VAG */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->loop_start_sample = read_32bitBE(0x44,streamFile);
            vgmstream->loop_end_sample = read_32bitBE(0x48,streamFile);
            /* todo 6ch has 0x8000 blocks and must skip last 0x20 each block (or, skip 0x20 every 0x1550*6 */

            break;

#ifdef VGM_USE_FFMPEG
        case 1: { /* XMA2 */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[FAKE_RIFF_BUFFER_SIZE];
            int32_t bytes, block_size, block_count;
            /* todo not accurate (needed for >2ch) */
            datasize = get_streamfile_size(streamFile)-start_offset;
            block_size = 2048;
            block_count = datasize / block_size; /* read_32bitLE(custom_data_offset +0x14) -1? */

            bytes = ffmpeg_make_riff_xma2(buf, FAKE_RIFF_BUFFER_SIZE, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            if (bytes <= 0) goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = read_32bitBE(0x44,streamFile);
            vgmstream->loop_end_sample = read_32bitBE(0x48,streamFile);

            break;
        }

        case 7: { /* ATRAC3 */
            uint8_t buf[FAKE_RIFF_BUFFER_SIZE];
            int32_t bytes, block_size, encoder_delay, joint_stereo, max_samples;

            datasize = read_32bitBE(0x54,streamFile);
            block_size = 0x98 * vgmstream->channels;
            joint_stereo = 0;
            max_samples = atrac3_bytes_to_samples(datasize, block_size);
            encoder_delay = 0x0; //max_samples - vgmstream->num_samples; /* todo not correct */
            vgmstream->num_samples = max_samples; /* use calc samples since loop points are too, breaks looping in some files otherwise */

            /* make a fake riff so FFmpeg can parse the ATRAC3 */
            bytes = ffmpeg_make_riff_atrac3(buf, FAKE_RIFF_BUFFER_SIZE, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
            if (bytes <= 0)
                goto fail;

            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
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


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
