#include "meta.h"

/* silent stream - mainly for engines that need them or dummy subsongs */
VGMSTREAM* init_vgmstream_silence(int channels, int sample_rate, int32_t num_samples) {
    VGMSTREAM* vgmstream = NULL;

    if (channels <= 0)
        channels = 2;
    if (sample_rate <= 0)
        sample_rate = 48000;
    if (num_samples <= 0)
        num_samples = 1.0 * sample_rate;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SILENCE;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_SILENCE;
    vgmstream->layout_type = layout_none;

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* silent stream - for containers that have dummy streams but it's a hassle to detect/filter out */
VGMSTREAM* init_vgmstream_silence_container(int total_subsongs) {
    VGMSTREAM* vgmstream = NULL;

    vgmstream = init_vgmstream_silence(0, 0, 0);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", "dummy");

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
