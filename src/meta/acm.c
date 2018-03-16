#include "meta.h"
#include "../coding/coding.h"
#include "../coding/acm_decoder.h"

/* ACM - InterPlay infinity engine games [Planescape: Torment (PC), Baldur's Gate (PC)] */
VGMSTREAM * init_vgmstream_acm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag = 0, channel_count, sample_rate, num_samples;
    mus_acm_codec_data *data = NULL;


    /* checks */
    if (!check_extensions(streamFile, "acm"))
        goto fail;
    if (read_32bitBE(0x0,streamFile) != 0x97280301) /* header id */
        goto fail;


    /* init decoder */
    data = init_acm(1);
    if (!data) goto fail;

    /* open and parse the file before creating the vgmstream */
    {
        ACMStream *acm_stream = NULL;
        char filename[PATH_LIMIT];

        streamFile->get_name(streamFile,filename,sizeof(filename));
        if (acm_open_decoder(&acm_stream,streamFile,filename) != ACM_OK)
            goto fail;

        data->files[0] = acm_stream;

        channel_count = acm_stream->info.channels;
        sample_rate = acm_stream->info.rate;
        num_samples = acm_stream->total_values / acm_stream->info.channels;
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
