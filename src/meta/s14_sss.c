#include "../vgmstream.h"

#ifdef VGM_USE_G7221

#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .s14 and .sss - from Idolm@ster DS (and others?)
 * Raw 24kbit Siren 14 stream, s14 is mono and sss is
 * frame-interleaved stereo
 */

VGMSTREAM * init_vgmstream_s14_sss(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int channel_count;

    size_t file_size;

    /* check extension, case insensitive */
    /* this is all we have to go on, rsf is completely headerless */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (!strcasecmp("sss",filename_extension(filename)))
    {
        channel_count = 2;
    }
    else if (!strcasecmp("s14",filename_extension(filename)))
    {
        channel_count = 1;
    }
    else
    {
        goto fail;
    }

    file_size = get_streamfile_size(streamFile);

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = file_size/0x3c/channel_count*(32000/50);
    vgmstream->sample_rate = 32768;

    vgmstream->coding_type = coding_G7221C;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x3c;
    if (1 == channel_count)
    {
        vgmstream->meta_type = meta_S14;
    }
    else
    {
        vgmstream->meta_type = meta_SSS;
    }

    {
        int i;
        g7221_codec_data *data;

        /* one data structure per channel */
        data = malloc(sizeof(g7221_codec_data) * channel_count);
        if (!data)
        {
            goto fail;
        }
        memset(data,0,sizeof(g7221_codec_data) * channel_count);
        vgmstream->codec_data = data;

        for (i = 0; i < channel_count; i++)
        {
            /* Siren 14 == 14khz bandwidth */
            data[i].handle = g7221_init(vgmstream->interleave_block_size, 14000);
            if (!data[i].handle)
            {
                goto fail; /* close_vgmstream is able to clean up */
            }
        }
    }

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=0x3c*i;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

#endif
