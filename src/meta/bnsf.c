#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"
#include "../vgmstream.h"

/* Namco Bandai's Bandai Namco Sound Format/File (BNSF) */
/* similar to RIFX */

VGMSTREAM * init_vgmstream_bnsf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    off_t file_size = -1;
    uint32_t riff_size;
    uint32_t bnsf_form;
    enum {
        form_IS14 = UINT32_C(0x49533134),  /* IS14 */
    };

    int channel_count = 0;
    int sample_count = 0;
    int sample_rate = 0;
    int coding_type = -1;
    off_t start_offset = -1;

    int loop_flag = 0;
    off_t loop_start = -1;
    off_t loop_end = -1;
    uint32_t data_size = 0;
    uint32_t block_size = 0;
    uint32_t block_samples = 0;

    int FormatChunkFound = 0;
    int DataChunkFound = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bnsf",filename_extension(filename)))
    {
        goto fail;
    }

    /* check header */
    if ((uint32_t)read_32bitBE(0,streamFile)!=0x424E5346) /* BNSF */
        goto fail;

    /* check form */
    bnsf_form = read_32bitBE(8,streamFile);
    switch (bnsf_form)
    {
#ifdef VGM_USE_G7221
        case form_IS14:
            break;
#endif
        default:
            goto fail;
    }

    riff_size = read_32bitBE(4,streamFile);
    file_size = get_streamfile_size(streamFile);

    /* check for tructated RIFF */
    if (file_size < riff_size+8) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bitBE(current_chunk+4,streamFile);

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x73666d74:    /* "sfmt" */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    sample_rate = read_32bitBE(current_chunk+0x0c,streamFile);
                    channel_count = read_16bitBE(current_chunk+0x0a,streamFile);
                    // read_32bitBE(current_chunk+0x10,streamFile); // ?
                    // read_32bitBE(current_chunk+0x14,streamFile); // ?
                    block_size = read_16bitBE(current_chunk+0x18,streamFile);
                    block_samples = read_16bitBE(current_chunk+0x1a,streamFile);

                    /* I assume this is still the codec id, but as the codec is
                       specified by the BNSF "form" I've only seen this zero */
                    switch ((uint16_t)read_16bitBE(current_chunk+0x8,streamFile)) {
                        case 0:
                            break;
                        default:
                            goto fail;
                    }
                    break;
                case 0x73646174:    /* sdat */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
                case 0x6C6F6F70:    /* loop */
                    loop_flag = 1;
                    loop_start =
                        read_32bitBE(current_chunk+8, streamFile);
                    loop_end =
                        read_32bitBE(current_chunk+0xc,streamFile);
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    switch (bnsf_form) {
#ifdef VGM_USE_G7221
        case form_IS14:
            coding_type = coding_G7221C;
            sample_count = data_size/block_size*block_samples;

            break;
#endif
        default:
            goto fail;
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size/channel_count;

    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        vgmstream->loop_end_sample = loop_end;
    }
    vgmstream->meta_type = meta_BNSF;

#ifdef VGM_USE_G7221
    if (coding_G7221C == coding_type)
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
#endif

    /* open the file, set up each channel */
    {
        int i;

        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,
                STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!vgmstream->ch[0].streamfile) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset =
                start_offset+i*vgmstream->interleave_block_size;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
