#include "meta.h"
#include "../coding/coding.h"

/* XWMA - Microsoft WMA container [The Elder Scrolls: Skyrim (PC/X360), Hydrophobia (PC)]  */
VGMSTREAM * init_vgmstream_xwma(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t fmt_offset, data_offset, first_offset = 0xc;
    size_t fmt_size, data_size;
    int loop_flag, channel_count;


    /* checks */
    /* .xwma: standard
     * .xwm: The Elder Scrolls: Skyrim (PC), Blade Arcus from Shining (PC) */
    if (!check_extensions(streamFile, "xwma,xwm"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x58574D41) /* "XWMA" */
        goto fail;

    if ( !find_chunk_le(streamFile, 0x666d7420,first_offset,0, &fmt_offset,&fmt_size) ) /* "fmt "*/
        goto fail;
    if ( !find_chunk_le(streamFile, 0x64617461,first_offset,0, &data_offset,&data_size) ) /* "data"*/
        goto fail;

    channel_count = read_16bitLE(fmt_offset+0x02,streamFile);
    loop_flag  = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(fmt_offset+0x04, streamFile);
    vgmstream->meta_type = meta_XWMA;

    /* the main purpose of this meta is redoing the XWMA header to:
     * - redo header to fix XWMA with buggy bit rates so FFmpeg can play them ok
     * - skip seek table to fix FFmpeg buggy XWMA seeking (see init_seek)
     * - read num_samples correctly
     */

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        int bytes, avg_bps, block_align, wma_codec;

        avg_bps = read_32bitLE(fmt_offset+0x08, streamFile);
        block_align = (uint16_t)read_16bitLE(fmt_offset+0x0c, streamFile);
        wma_codec = (uint16_t)read_16bitLE(fmt_offset+0x00, streamFile);

        bytes = ffmpeg_make_riff_xwma(buf,0x100, wma_codec, data_size, vgmstream->channels, vgmstream->sample_rate, avg_bps, block_align);
        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, data_offset,data_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* manually find total samples, why don't they put this in the header is beyond me */
        {
            ms_sample_data msd = {0};

            msd.channels = vgmstream->channels;
            msd.data_offset = data_offset;
            msd.data_size = data_size;

            if (wma_codec == 0x0162)
                wmapro_get_samples(&msd, streamFile, block_align, vgmstream->sample_rate,0x00E0);
            else
                wma_get_samples(&msd, streamFile, block_align, vgmstream->sample_rate,0x001F);

            vgmstream->num_samples = msd.num_samples;
            if (vgmstream->num_samples == 0)
                vgmstream->num_samples = ffmpeg_get_samples(vgmstream->codec_data); /* from avg-br */
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
