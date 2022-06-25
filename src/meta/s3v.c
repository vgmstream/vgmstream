#include "meta.h"
#include "../coding/coding.h"

/* S3V - from Konami arcade games [SOUND VOLTEX series (AC)] */
VGMSTREAM * init_vgmstream_s3v(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
	int32_t channel_count, loop_flag;
    size_t data_size;

    /* checks */
    if (!check_extensions(sf, "s3v"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,sf) != 0x53335630) /* S3V0 */
        goto fail;

    /* No discernible loop_flag so we'll just use signatures.
       Might have to update on a per game basis. */
	switch (read_32bitBE(0x14, sf)) {
        case 0x82FA0000: // SOUND VOLTEX EXCEED GEAR ver5 Theme BGM
        case 0x1BFD0000: // SOUND VOLTEX EXCEED GEAR ver6 Theme BGM
        case 0x9AFD0000: // SOUND VOLTEX Custom song selection BGM TypeA
        case 0x9BFD0000: // SOUND VOLTEX Custom song selection BGM TypeB
            loop_flag = 1;
            break;

        default:
            loop_flag = 0;
    }

    start_offset = 0x20;
    data_size = read_32bitLE(0x08, sf);
    channel_count = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;
    vgmstream->meta_type = meta_S3V;

#ifdef VGM_USE_FFMPEG

    vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset, data_size);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->sample_rate = ffmpeg_get_sample_rate(vgmstream->codec_data);
    vgmstream->num_samples = ffmpeg_get_samples(vgmstream->codec_data);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x10, sf);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;

#endif

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
