#include "meta.h"
#include "../coding/coding.h"
#include "../coding/acm_decoder.h"

/* ACM - InterPlay infinity engine games [Planescape: Torment (PC), Baldur's Gate (PC)] */
VGMSTREAM * init_vgmstream_acm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag = 0, channel_count, sample_rate, num_samples;
    acm_codec_data *data = NULL;


    /* checks */
    if (!check_extensions(streamFile, "acm"))
        goto fail;
    if (read_32bitBE(0x0,streamFile) != 0x97280301) /* header id */
        goto fail;


    /* init decoder */
    {
        data = init_acm(streamFile);
        if (!data) goto fail;

        channel_count = data->file->info.channels;
        sample_rate = data->file->info.rate;
        num_samples = data->file->total_values / data->file->info.channels;
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
