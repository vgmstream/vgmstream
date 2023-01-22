#include "meta.h"
#include "../coding/coding.h"

/* PASX - from Premium Agency games [SoulCalibur II HD (X360), Death By Cube (X360)] */
VGMSTREAM* init_vgmstream_pasx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, chunk_offset;
    size_t data_size, chunk_size;
    int loop_flag, channels, sample_rate;
    int num_samples, loop_start_sample, loop_end_sample;


    /* checks */
    if (!is_id32be(0x00,sf, "PASX"))
        goto fail;

    /* .past: Soul Calibur II HD
     * .sgb: Death By Cube */
    if (!check_extensions(sf,"past,sgb"))
        goto fail;


    /* custom header with a "fmt " data chunk inside */
    chunk_size   = read_32bitBE(0x08,sf);
    data_size    = read_32bitBE(0x0c,sf);
    chunk_offset = read_32bitBE(0x10,sf); /* 0x14: fmt offset end */
    start_offset = read_32bitBE(0x18,sf);

    channels = read_16bitBE(chunk_offset+0x02,sf);
    sample_rate   = read_32bitBE(chunk_offset+0x04,sf);
    xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_PASX;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, chunk_offset, chunk_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, chunk_offset, 1,1);
    }
#else
    goto fail;
#endif


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
