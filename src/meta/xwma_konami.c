#include "meta.h"
#include "../coding/coding.h"
#include "xwma_konami_streamfile.h"


/* MSFC - Konami (Armature?) variation [Metal Gear Solid 2 HD (X360), Metal Gear Solid 3 HD (X360)] */
VGMSTREAM* init_vgmstream_xwma_konami(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, codec, sample_rate;
    size_t data_size;
    STREAMFILE *temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "XWMA"))
        goto fail;
    if (!check_extensions(sf,"xwma"))
        goto fail;

    codec = read_32bitBE(0x04,sf);
    channels = read_32bitBE(0x08,sf);
    sample_rate = read_32bitBE(0x0c,sf);
    data_size = read_32bitBE(0x10,sf); /* data size without padding */
    loop_flag  = 0;
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_XWMA_KONAMI;

#ifdef VGM_USE_FFMPEG
    {
        /* 0x10: related to size? */
        int avg_bps     = read_32bitBE(0x14, sf);
        int block_align = read_32bitBE(0x18, sf);

        /* data has padding (unrelated to KCEJ blocks) */
        temp_sf = setup_xwma_konami_streamfile(sf, start_offset, block_align);
        if (!temp_sf) goto fail;

        vgmstream->codec_data = init_ffmpeg_xwma(temp_sf, 0x00, data_size, codec, channels, sample_rate, avg_bps, block_align);
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
                wma_get_samples(&msd, temp_sf, block_align, vgmstream->sample_rate,0x001F);
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

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
