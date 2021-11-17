#include "meta.h"
#include "../coding/coding.h"


/* .STRM - from Abylight 3DS games [Cursed Castilla (3DS)] */
VGMSTREAM* init_vgmstream_strm_abylight(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, skip_samples;
    size_t data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "STRM"))
        goto fail;

    if (!check_extensions(sf,"strm"))
        goto fail;

    if (read_32bitLE(0x04,sf) != 0x03E8) /* version 1000? */
        goto fail;

    loop_flag = 0;
    channel_count = 2; /* there are various possible fields but all files are stereo */
    sample_rate = read_32bitLE(0x08,sf);
    skip_samples = 1024; /* assumed, maybe a bit more */

    start_offset = 0x1e;
    data_size = read_32bitLE(0x10,sf);
    if (data_size != get_streamfile_size(sf) - start_offset)
        goto fail;
    if (data_size != read_32bitLE(0x18,sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = aac_get_samples(sf, start_offset, data_size);

    vgmstream->meta_type = meta_STRM_ABYLIGHT;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_aac(sf, start_offset, data_size, skip_samples);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        vgmstream->num_samples -= skip_samples;
    }
#else
    goto fail;
#endif

    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
