#include "meta.h"
#include "../coding/coding.h"
#include "mzrt_streamfile.h"


/* mzrt - idTech "4.5" audio found in .resource bigfiles (w/ internal filenames) [Doom 3 BFG edition (PC/PS3/X360)] */
VGMSTREAM * init_vgmstream_mzrt(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, codec, sample_rate, block_size = 0, bps = 0, num_samples;
    STREAMFILE *temp_streamFile = NULL;


    /* checks */
    if (!check_extensions(streamFile, "idwav,idmsf,idxma"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x6D7A7274) /* "mzrt" */
        goto fail;

    /* this format is bizarrely mis-aligned (and mis-designed too) */

    num_samples = read_32bitBE(0x11,streamFile);
    codec = read_16bitLE(0x15,streamFile);
    switch(codec) {
        case 0x0001:
        case 0x0002:
        case 0x0166:
            channel_count = read_16bitLE(0x17,streamFile);
            sample_rate = read_32bitLE(0x19, streamFile);
            block_size = read_16bitLE(0x21, streamFile);
            bps = read_16bitLE(0x23,streamFile);

            start_offset = 0x25;
            break;

        case 0x0000:
            sample_rate = read_32bitBE(0x1D, streamFile);
            channel_count = read_32bitBE(0x21, streamFile);

            start_offset = 0x29;
            break;

        default:
            goto fail;
    }

    /* skip MSADPCM data */
    if (codec == 0x0002) {
        if (!msadpcm_check_coefs(streamFile, start_offset + 0x02 + 0x02))
            goto fail;

        start_offset += 0x02 + read_16bitLE(start_offset, streamFile);
    }

    /* skip extra data */
    if (codec == 0x0166) {
        start_offset += 0x02 + read_16bitLE(start_offset, streamFile);
    }

    /* skip unknown table */
    if (codec == 0x0000) {
        start_offset += 0x04 + read_32bitBE(start_offset, streamFile) * 0x04;
    }

    /* skip unknown table */
    start_offset += 0x04 + read_32bitBE(start_offset, streamFile);

    /* skip block info */
    if (codec != 0x0000) {
        /* 0x00: de-blocked size
         * 0x04: block count*/
        start_offset += 0x08;

        /* idwav only uses 1 super-block though */
        temp_streamFile = setup_mzrt_streamfile(streamFile, start_offset);
        if (!temp_streamFile) goto fail;
    }
    else {
        /* 0x00: de-blocked size */
        start_offset += 0x04;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MZRT;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    switch(codec) {
        case 0x0001:
            if (bps != 16) goto fail;
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = block_size / channel_count;
            break;

        case 0x0002:
            if (bps != 4) goto fail;
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = block_size;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0166: {
            uint8_t buf[0x100];
            int bytes;
            size_t stream_size = get_streamfile_size(temp_streamFile);

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,sizeof(buf), 0x15,0x34, stream_size, streamFile, 0);
            vgmstream->codec_data = init_ffmpeg_header_offset(temp_streamFile, buf,bytes, 0x00,stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples_hb(vgmstream, streamFile, temp_streamFile, 0x00,stream_size, 0x15, 0,0);
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0000: {
            mpeg_custom_config cfg = {0};

            cfg.skip_samples = 576; /* assumed */

            vgmstream->codec_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,temp_streamFile == NULL ? streamFile : temp_streamFile,temp_streamFile == NULL ? start_offset : 0x00))
        goto fail;
    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
