#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"


/* OGL - Shin'en custom Vorbis [Jett Rocket (Wii), FAST Racing NEO (WiiU)] */
VGMSTREAM * init_vgmstream_ogl(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t partial_file_size;
    int loop_flag, channel_count, sample_rate;
    uint32_t num_samples, loop_start_sample, loop_end_sample;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"ogl"))
        goto fail;

    /* OGL headers are very basic with no ID but libvorbis should reject garbage data anyway */
    loop_flag           = read_32bitLE(0x00,streamFile) > 0; /* absolute loop offset */
    loop_start_sample   = read_32bitLE(0x04,streamFile);
    //loop_start_block  = read_32bitLE(0x08,streamFile);
    num_samples         = read_32bitLE(0x0c,streamFile);
    partial_file_size   = read_32bitLE(0x10,streamFile); /* header + data not counting end padding */
    if (partial_file_size > get_streamfile_size(streamFile)) goto fail;
    loop_end_sample = num_samples; /* there is no data after num_samples (ie.- it's really num_samples) */

    /* this is actually peeking into the Vorbis id packet */
    channel_count   = read_8bit   (0x21,streamFile);
    sample_rate     = read_32bitLE(0x22,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples       = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_OGL;

#ifdef VGM_USE_VORBIS
    {
        vgmstream->codec_data = init_ogl_vorbis_codec_data(streamFile, 0x14, &start_offset);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_ogl_vorbis;
        vgmstream->layout_type = layout_none;
    }
#else
    goto fail;
#endif

    /* non-looping files do this */
    if (!num_samples) {
        uint32_t avg_bitrate = read_32bitLE(0x2a,streamFile); /* inside id packet */
        /* approximate as we don't know the sizes of all packet headers */ //todo this is wrong... but somehow works?
        vgmstream->num_samples = (partial_file_size - start_offset) * ((sample_rate*10/avg_bitrate)+1);
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
