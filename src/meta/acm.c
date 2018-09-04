#include "meta.h"
#include "../coding/coding.h"
#include "../coding/acm_decoder_libacm.h"

/* ACM - InterPlay infinity engine games [Planescape: Torment (PC), Baldur's Gate (PC)] */
VGMSTREAM * init_vgmstream_acm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag = 0, channel_count, sample_rate, num_samples;
    int force_channel_number = 0;
    acm_codec_data *data = NULL;


    /* checks */
    /* .acm: plain ACM extension (often but not always paired with .mus, parsed elsewhere)
     * .wavc: header id for WAVC sfx (from bigfiles, extensionless) */
    if (!check_extensions(streamFile, "acm,wavc"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x97280301 &&  /* header id (music) */
        read_32bitBE(0x00,streamFile) != 0x57415643)    /* "WAVC" (sfx) */
        goto fail;


    /* Plain ACM "channels" in the header may be set to 2 for mono voices or 1 for music,
     * but actually seem related to ACM rows/cols and have nothing to do with channels.
     *
     * libacm will set plain ACM (not WAVC) to 2ch unless changes unless changed, but
     * only Fallout (PC) seems to use plain ACM for sfx, others are WAVC (which do have channels).
     *
     * Doesn't look like there is any way to detect mono/stereo, so as a quick hack if
     * we have a plain ACM (not WAVC) named .wavc we will force 1ch. */
    if (check_extensions(streamFile, "wavc")
            && read_32bitBE(0x00,streamFile) == 0x97280301) {
        force_channel_number = 1;
    }

    /* init decoder */
    {
        ACMStream *handle;
        data = init_acm(streamFile, force_channel_number);
        if (!data) goto fail;

        handle = data->handle;
        channel_count = handle->info.channels;
        sample_rate = handle->info.rate;
        num_samples = handle->total_values / handle->info.channels;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->meta_type = meta_ACM;
    vgmstream->coding_type = coding_ACM;
    vgmstream->layout_type = layout_none;

    vgmstream->codec_data = data;

    return vgmstream;

fail:
    free_acm(data);
    close_vgmstream(vgmstream);
    return NULL;
}
