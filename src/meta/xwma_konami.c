#include "meta.h"
#include "../coding/coding.h"


/* MSFC - Konami (Armature?) variation [Metal Gear Solid 2 HD (X360), Metal Gear Solid 3 HD (X360)] */
VGMSTREAM * init_vgmstream_xwma_konami(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, codec, sample_rate;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile,"xwma"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x58574D41) /* "XWMA" */
        goto fail;

    codec = read_32bitBE(0x04,streamFile);
    channel_count = read_32bitBE(0x08,streamFile);
    sample_rate = read_32bitBE(0x0c,streamFile);
    data_size = read_32bitBE(0x10,streamFile);
    loop_flag  = 0;
    start_offset = 0x20;
    //if (data_size + start_offset != get_streamfile_size(streamFile))
    //    goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_XWMA_KONAMI;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        int bytes, avg_bps, block_align;

        /* 0x10: related to size? */
        avg_bps     = read_32bitBE(0x14, streamFile);
        block_align = read_32bitBE(0x18, streamFile);

        bytes = ffmpeg_make_riff_xwma(buf,0x100, codec, data_size, channel_count, sample_rate, avg_bps, block_align);
        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* manually find total samples */
        {
            ms_sample_data msd = {0};

            msd.channels = vgmstream->channels;
            msd.data_offset = start_offset;
            msd.data_size = data_size;

            if (codec == 0x0162)
                ;//wmapro_get_samples(&msd, streamFile, block_align, vgmstream->sample_rate,0x00E0); //todo not correct
            else
                wma_get_samples(&msd, streamFile, block_align, vgmstream->sample_rate,0x001F);

            vgmstream->num_samples = msd.num_samples;
            if (vgmstream->num_samples == 0)
                vgmstream->num_samples = (int32_t)((ffmpeg_codec_data*)vgmstream->codec_data)->totalSamples; /* from avg-br */
            //num_samples seem to be found in the last "seek" table entry too, as: entry / channels / 2
        }
    }
#else
    goto fail;
#endif

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
