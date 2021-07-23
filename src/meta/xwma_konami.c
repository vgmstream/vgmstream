#include "meta.h"
#include "../coding/coding.h"
#include "xwma_konami_streamfile.h"


/* MSFC - Konami (Armature?) variation [Metal Gear Solid 2 HD (X360), Metal Gear Solid 3 HD (X360)] */
VGMSTREAM * init_vgmstream_xwma_konami(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, codec, sample_rate;
    size_t data_size;
    STREAMFILE *temp_streamFile = NULL;


    /* checks */
    if (!check_extensions(streamFile,"xwma"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x58574D41) /* "XWMA" */
        goto fail;

    codec = read_32bitBE(0x04,streamFile);
    channel_count = read_32bitBE(0x08,streamFile);
    sample_rate = read_32bitBE(0x0c,streamFile);
    data_size = read_32bitBE(0x10,streamFile); /* data size without padding */
    loop_flag  = 0;
    start_offset = 0x20;


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

        /* data has padding (unrelated to KCEJ blocks) */
        temp_streamFile = setup_xwma_konami_streamfile(streamFile, start_offset, block_align);
        if (!temp_streamFile) goto fail;

        bytes = ffmpeg_make_riff_xwma(buf,0x100, codec, data_size, channel_count, sample_rate, avg_bps, block_align);
        vgmstream->codec_data = init_ffmpeg_header_offset(temp_streamFile, buf,bytes, 0x00,data_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* manually find total samples */
        {
            ms_sample_data msd = {0};

            msd.channels = vgmstream->channels;
            msd.data_offset = 0x00;
            msd.data_size = data_size;


            if (codec == 0x0161)
                wma_get_samples(&msd, temp_streamFile, block_align, vgmstream->sample_rate,0x001F);
            //else //todo not correct
            //    wmapro_get_samples(&msd, temp_streamFile, block_align, vgmstream->sample_rate,0x00E0);

            vgmstream->num_samples = msd.num_samples;
            if (vgmstream->num_samples == 0)
                vgmstream->num_samples = ffmpeg_get_samples(vgmstream->codec_data); /* from avg-br */
            //num_samples seem to be found in the last "seek" table entry too, as: entry / channels / 2
        }
    }
#else
    goto fail;
#endif

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
