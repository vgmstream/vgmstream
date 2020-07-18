#include "meta.h"
#include "../coding/coding.h"

/* .NAAC - from Namco 3DS games (Ace Combat - Assault Horizon Legacy, Taiko no Tatsujin Don to Katsu no Jikuu Daibouken) */
VGMSTREAM* init_vgmstream_naac(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;

    /* check extension */
    if ( !check_extensions(sf,"naac") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,sf) != 0x41414320) /* "AAC " */
        goto fail;
    if (read_32bitLE(0x04,sf) != 0x01) /* version? */
        goto fail;

    start_offset = 0x1000;
    loop_flag = (read_32bitLE(0x18,sf) != 0);
    channel_count = read_32bitLE(0x08,sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x0c,sf);
    vgmstream->num_samples = read_32bitLE(0x10,sf); /* without skip_samples */
    vgmstream->loop_start_sample = read_32bitLE(0x14,sf); /* with skip_samples */
    vgmstream->loop_end_sample = read_32bitLE(0x18,sf);
    /* 0x1c: loop start offset, 0x20: loop end offset (within data) */
    data_size = read_32bitLE(0x24,sf);
    /* 0x28: unknown;  0x2c: table start offset?;  0x30: seek table (always 0xFD0, padded) */

    vgmstream->meta_type = meta_NAAC;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset,data_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* observed default, some files start without silence though seems correct when loop_start=0 */
        ffmpeg_set_skip_samples(vgmstream->codec_data, 1024); /* raw AAC doesn't set this */
        vgmstream->num_samples -= 1024;
    }
#else
    goto fail;
#endif

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
