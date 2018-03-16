#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"
#include "../coding/acm_decoder.h"

/* InterPlay ACM */
/* The real work is done by libacm */
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
    {
        data = calloc(1,sizeof(mus_acm_codec_data));
        if (!data) goto fail;

        data->current_file = 0;
        data->file_count = 1;
        data->files = calloc(data->file_count,sizeof(ACMStream *));
        if (!data->files) goto fail;
    }

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
    if (data && (!vgmstream || !vgmstream->codec_data)) {
        if (data) {
            int i;
            for (i = 0; i < data->file_count; i++) {
                acm_close(data->files[i]);
            }
            free(data->files);
            free(data);
        }
    }
    close_vgmstream(vgmstream);
    return NULL;
}
