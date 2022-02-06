#include "meta.h"
#include "../coding/coding.h"


/* MPEG - standard MP1/2/3 audio MP3 */
VGMSTREAM* init_vgmstream_mpeg(STREAMFILE* sf) {
#ifdef VGM_USE_MPEG
    VGMSTREAM* vgmstream = NULL;
    int loop_flag = 0;
    mpeg_frame_info info = {0};


    /* checks */
    if (!mpeg_get_frame_info(sf, 0x00, &info))
        goto fail;

    /*  .mp3/mp2: standard (is .mp1 ever used in games?)
     * .lmp1/2/3: for plugins
     * .mus: Marc Ecko's Getting Up (PC) */
    if (!check_extensions(sf, "mp3,mp2,mp1,mus,lmp3,lmp2,lmp1"))
        goto fail;

    loop_flag = 0;


    /* build VGMSTREAM */
    vgmstream = allocate_vgmstream(info.channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MPEG;
    vgmstream->sample_rate = info.sample_rate;

    /* more strict, use? */
    //mpeg_custom_config cfg = {0};
    //cfg.skip_samples = ...
    //vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, fmt.channels, MPEG_STANDARD, &cfg);

    vgmstream->codec_data = init_mpeg(sf, 0x00, &vgmstream->coding_type, info.channels);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->layout_type = layout_none;

    //vgmstream->num_samples = mpeg_bytes_to_samples(data_size, vgmstream->codec_data);
    vgmstream->num_samples = mpeg_get_samples(sf, 0x00, get_streamfile_size(sf));


    if (!vgmstream_open_stream(vgmstream, sf, 0x00))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
#else
    return NULL;
#endif
}
