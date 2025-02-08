#include "meta.h"
#include "../coding/coding.h"


/* MPEG - standard MP1/2/3 audio */
VGMSTREAM* init_vgmstream_mpeg(STREAMFILE* sf) {
#ifdef VGM_USE_MPEG
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;


    /* checks */
    uint32_t header_id = read_u32be(0x00, sf);
    if ((header_id & 0xFFE00000) != 0xFFE00000 &&
        (header_id & 0xFFFFFF00) != get_id32be("ID3\0") &&
        (header_id & 0xFFFFFF00) != get_id32be("TAG\0"))
        return NULL;

    /* detect base offset, since some tags with images are big
     * (init_mpeg only skips tags in a small-ish buffer) */
    start_offset = 0x00;
    while (start_offset < get_streamfile_size(sf)) {
        uint32_t tag_size = mpeg_get_tag_size(sf, start_offset, 0);
        if (tag_size == 0)
            break;
        start_offset += tag_size;
    }

    // could check size but there are some tiny <0x1000 low bitrate files do exist in flash games

    mpeg_frame_info info = {0};
    if (!mpeg_get_frame_info(sf, start_offset, &info))
        return NULL;

    /* .mp3/mp2: standard
     * .lmp3/lmp2: for plugins
     * .mus: Marc Ecko's Getting Up (PC) 
     * .imf: Colors (Gizmondo)
     * .aix: Classic Compendium 2 (Gizmondo)
     * .wav/lwav: The Seventh Seal (PC)
     * .nfx: Grand Theft Auto III (Android)
     * (extensionless): Interstellar Flames 2 (Gizmondo) */
    if (!check_extensions(sf, "mp3,mp2,lmp3,lmp2,mus,imf,aix,wav,lwav,nfx,"))
        return NULL;

    bool loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(info.channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MPEG;
    vgmstream->sample_rate = info.sample_rate;

    /* more strict, use? */
    //mpeg_custom_config cfg = {0};
    //cfg.skip_samples = ...
    //vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, fmt.channels, MPEG_STANDARD, &cfg);

    vgmstream->codec_data = init_mpeg(sf, start_offset, &vgmstream->coding_type, info.channels);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->layout_type = layout_none;

    //vgmstream->num_samples = mpeg_bytes_to_samples(data_size, vgmstream->codec_data);
    vgmstream->num_samples = mpeg_get_samples(sf, start_offset, get_streamfile_size(sf));


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
#else
    return NULL;
#endif
}
