#include "meta.h"
#include "../coding/coding.h"
#include "../coding/acm_decoder_libacm.h"

/* ACM - InterPlay infinity engine games [Planescape: Torment (PC), Baldur's Gate (PC)] */
VGMSTREAM* init_vgmstream_acm(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag = 0, channels, sample_rate, num_samples;
    int force_channel_number = 0;
    acm_codec_data *data = NULL;


    /* checks */
    /* .acm: plain ACM extension (often but not always paired with .mus, parsed elsewhere)
     * .tun: Descent to Undermountain (PC)
     * .wavc: header id for WAVC sfx (from bigfiles, extensionless) */
    if (!check_extensions(sf, "acm,tun,wavc"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x97280301 &&  /* header id (music) */
        !is_id32be(0x00,sf, "WAVC"))    /* sfx */
        goto fail;


    /* Plain ACM "channels" in the header (at 0x08) may be set to 2 for mono voices [FO1] or 1 for music [P:T],
     * but actually seem related to ACM rows/cols and have nothing to do with channels.
     *
     * libacm will set plain ACM (not WAVC) to 2ch unless changed, but only Fallout (PC)
     * and Descent to Undermountain (PC) seems to use plain ACM for sfx/voices,
     * others are WAVC (which do have channels).
     * DtU seems to use the field 
     *
     * Doesn't look like there is any way to detect mono/stereo, so as a quick hack if
     * we have a plain ACM (not WAVC) named .wavc we will force 1ch. */
    if (check_extensions(sf, "wavc") && read_u32be(0x00,sf) == 0x97280301) {
        force_channel_number = 1;
    }

    /* init decoder */
    {
        ACMStream *handle;
        data = init_acm(sf, force_channel_number);
        if (!data) goto fail;

        handle = data->handle;
        channels = handle->info.channels;
        sample_rate = handle->info.rate;
        num_samples = handle->total_values / handle->info.channels;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
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
