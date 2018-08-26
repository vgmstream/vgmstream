#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* GSP+GSB - from Tecmo's Super Swing Golf 1 & 2 (Wii), Quantum Theory (PS3/X360) */
VGMSTREAM * init_vgmstream_gsp_gsb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    int loop_flag, channel_count;
    off_t start_offset, chunk_offset, first_offset;
    size_t datasize;
    int codec_id;
	
    
    /* check extensions */
    if (!check_extensions(streamFile,"gsb"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile, "gsp");
    if (!streamHeader) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamHeader) != 0x47534E44)	/* "GSND" */
        goto fail;
    /*  0x04: version?,  0x08: 1?,  0x0c: 0?,  */
    first_offset = read_32bitBE(0x10,streamHeader); /* usually 0x14*/

    if (!find_chunk_be(streamHeader, 0x44415441,first_offset,1, &chunk_offset,NULL)) goto fail; /*"DATA"*/
    channel_count = read_16bitBE(chunk_offset+0x0e,streamHeader);
    if (!find_chunk_be(streamHeader, 0x42534943,first_offset,1, &chunk_offset,NULL)) goto fail; /*"BSIC"*/
    loop_flag = read_8bit(chunk_offset+0x0c,streamHeader);

    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GSP_GSB;


    if (!find_chunk_be(streamHeader, 0x48454144,first_offset,1, &chunk_offset,NULL)) goto fail; /*"HEAD"*/
    /*  0x00: header_size,  0x04: num_chunks */

    if (!find_chunk_be(streamHeader, 0x44415441,first_offset,1, &chunk_offset,NULL)) goto fail; /*"DATA"*/
    /*  0x00: filesize,  0x0c: always 10?,  0x10: always 0?,  0x18: always 0? */
    datasize = read_32bitBE(chunk_offset+0x00,streamHeader);
    codec_id = read_32bitBE(chunk_offset+0x04,streamHeader);
    vgmstream->sample_rate = read_32bitBE(chunk_offset+0x08,streamHeader);
    vgmstream->num_samples = read_32bitBE(chunk_offset+0x14,streamHeader);
    /*  0x1c: unk (varies with codec_id) */

    if (!find_chunk_be(streamHeader, 0x42534943,first_offset,1, &chunk_offset,NULL)) goto fail; /*"BSIC"*/
    /*  0x00+: probably volume/pan/etc */
    vgmstream->loop_start_sample = read_32bitBE(chunk_offset+0x10,streamHeader);
    vgmstream->loop_end_sample = read_32bitBE(chunk_offset+0x14,streamHeader);

    //if (!find_chunk_be(streamHeader, 0x4E414D45,first_offset,1, &chunk_offset,NULL)) goto fail; /*"NAME"*/
    /* 0x00: name_size, 0x04+: name*/

    switch (codec_id) {
        case 0x04: { /* DSP [Super Swing Golf (Wii)] */
            size_t block_header_size;
            size_t num_blocks;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_gsb;

            if (!find_chunk_be(streamHeader, 0x47434558,first_offset,1, &chunk_offset,NULL)) goto fail; /*"GCEX"*/

            //vgmstream->current_block_size = read_32bitBE(chunk_offset+0x00,streamHeader);
            block_header_size = read_32bitBE(chunk_offset+0x04,streamHeader);
            num_blocks = read_32bitBE(chunk_offset+0x08,streamHeader);
            vgmstream->num_samples = (datasize - block_header_size * num_blocks) / 8 / vgmstream->channels * 14;
            /* 0x0c+: unk */

            dsp_read_coefs_be(vgmstream, streamHeader, chunk_offset+0x18, 0x30);
            break;
        }
#ifdef VGM_USE_FFMPEG
        case 0x08: { /* ATRAC3 [Quantum Theory (PS3)] */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[100];
            int32_t bytes, block_size, encoder_delay, joint_stereo, max_samples;

            block_size    = 0x98 * vgmstream->channels;
            joint_stereo  = 0;
            max_samples   = atrac3_bytes_to_samples(datasize, block_size);;
            encoder_delay = max_samples - vgmstream->num_samples; /* todo guessed */

            vgmstream->num_samples += encoder_delay;
            /* make a fake riff so FFmpeg can parse the ATRAC3 */
            bytes = ffmpeg_make_riff_atrac3(buf,100, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
            if (bytes <= 0)
                goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = (vgmstream->loop_start_sample / ffmpeg_data->blockAlign) * ffmpeg_data->frameSize;
            vgmstream->loop_end_sample = (vgmstream->loop_end_sample / ffmpeg_data->blockAlign) * ffmpeg_data->frameSize;

            break;
        }

        case 0x09: { /* XMA2 [Quantum Theory (PS3)] */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[200];
            int32_t bytes;

            if (!find_chunk_be(streamHeader, 0x584D4558,first_offset,1, &chunk_offset,NULL)) goto fail; /*"XMEX"*/
            /* 0x00: fmt0x166 header (BE),  0x34: seek table */

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,200, chunk_offset,0x34, datasize, streamHeader, 1);
            if (bytes <= 0) goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif
        default:
            goto fail;
    }


    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
