#include "meta.h"
#include "../coding/coding.h"


/* .NAAC - from Namco 3DS games (Ace Combat - Assault Horizon Legacy, Taiko no Tatsujin Don to Katsu no Jikuu Daibouken) */
VGMSTREAM* init_vgmstream_naac(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, skip_samples;
    size_t data_size;


    /* checks */
    if (!check_extensions(sf,"naac"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x41414320) /* "AAC " */
        goto fail;
    if (read_u32le(0x04,sf) != 0x01) /* version? */
        goto fail;

    start_offset = 0x1000;
    loop_flag = (read_s32le(0x18,sf) != 0);
    channels = read_s32le(0x08,sf);
    skip_samples = 1024; /* raw AAC doesn't set this */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x0c,sf);
    vgmstream->num_samples = read_s32le(0x10,sf); /* without skip_samples */
    vgmstream->loop_start_sample = read_s32le(0x14,sf); /* with skip_samples */
    vgmstream->loop_end_sample = read_s32le(0x18,sf) + 1; /* without skip_samples */
    /* 0x1c: loop start offset */
    /* 0x20: loop end offset (within data) */
    data_size = read_u32le(0x24,sf);
    /* 0x28: unknown */
    /* 0x2c: table start offset? */
    /* 0x30: seek table (always 0xFD0, padded) */

    vgmstream->meta_type = meta_NAAC;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_aac(sf, start_offset, data_size, skip_samples);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* observed default, some files start without silence though seems correct when loop_start=0 */
        vgmstream->num_samples -= skip_samples;
        vgmstream->loop_end_sample -= skip_samples;
        /* for some reason last frame is ignored/bugged in various decoders (gives EOF errors) */
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
