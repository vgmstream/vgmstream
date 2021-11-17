#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"


/* OGL - Shin'en custom Vorbis [Jett Rocket (Wii), FAST Racing NEO (WiiU)] */
VGMSTREAM* init_vgmstream_ogl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t partial_file_size;
    int loop_flag, channels, sample_rate;
    uint32_t num_samples, loop_start_sample, loop_end_sample;

    /* checks */
    if (read_u32le(0x00, sf) > 0x10000000) /* limit loop samples (should catch fourccs) */
        goto fail;
    if (!is_id32be(0x17, sf, "vorb")) /* Vorbis id packet */
        goto fail;

    if (!check_extensions(sf,"ogl"))
        goto fail;

    /* OGL headers are very basic with no ID but libvorbis should reject garbage data anyway */
    loop_flag           = read_s32le(0x00,sf) > 0; /* absolute loop offset */
    loop_start_sample   = read_s32le(0x04,sf);
    //loop_start_block  = read_s32le(0x08,streamFile);
    num_samples         = read_s32le(0x0c,sf);
    partial_file_size   = read_s32le(0x10,sf); /* header + data not counting end padding */
    if (partial_file_size > get_streamfile_size(sf))
        goto fail;
    loop_end_sample = num_samples; /* there is no data after num_samples (ie.- it's really num_samples) */

    /* actually peeking into the Vorbis id packet */
    channels            = read_u8   (0x21,sf);
    sample_rate         = read_s32le(0x22,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate       = sample_rate;
    vgmstream->num_samples       = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_OGL;

#ifdef VGM_USE_VORBIS
    {
        vorbis_custom_config cfg = {0};

        vgmstream->layout_type = layout_none;
        vgmstream->coding_type = coding_VORBIS_custom;
        vgmstream->codec_data = init_vorbis_custom(sf, 0x14, VORBIS_OGL, &cfg);
        if (!vgmstream->codec_data) goto fail;

        start_offset = cfg.data_start_offset;
    }
#else
    goto fail;
#endif

    /* non-looping files do this */
    if (!num_samples) {
        uint32_t avg_bitrate = read_u32le(0x2a,sf); /* inside id packet */
        /* approximate as we don't know the sizes of all packet headers */ //todo this is wrong... but somehow works?
        vgmstream->num_samples = (partial_file_size - start_offset) * ((sample_rate*10/avg_bitrate)+1);
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
