#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"

/* Resource Interchange File Format */

/* return milliseconds */
long parse_marker(unsigned char * marker) {
    long hh,mm,ss,ms;
    if (memcmp("Marker ",marker,7)) return -1;

    if (4 != sscanf((char*)marker+7,"%ld:%ld:%ld.%ld",&hh,&mm,&ss,&ms))
        return -1;

    return ((hh*60+mm)*60+ss)*1000+ms;
}

/* loop points have been found hiding here */
void parse_adtl(off_t adtl_offset, off_t adtl_length, STREAMFILE  *streamFile,
        long *loop_start, long *loop_end, int *loop_flag) {
    int loop_start_found = 0;
    int loop_end_found = 0;

    off_t current_chunk = adtl_offset+4;

    while (current_chunk < adtl_offset+adtl_length) {
        uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
        off_t chunk_size = read_32bitLE(current_chunk+4,streamFile);

        if (current_chunk+8+chunk_size > adtl_offset+adtl_length) return;

        switch(chunk_type) {
            case 0x6c61626c:    /* labl */
                {
                    unsigned char *labelcontent;
                    labelcontent = malloc(chunk_size-4);
                    if (!labelcontent) return;
                    if (read_streamfile(labelcontent,current_chunk+0xc,
                                chunk_size-4,streamFile)!=chunk_size-4) {
                        free(labelcontent);
                        return;
                    }

                    switch (read_32bitLE(current_chunk+8,streamFile)) {
                        case 1:
                            if (!loop_start_found &&
                                (*loop_start = parse_marker(labelcontent))>=0)
                            {
                                loop_start_found = 1;
                            }
                            break;
                        case 2:
                            if (!loop_end_found &&
                                    (*loop_end = parse_marker(labelcontent))>=0)
                            {
                                loop_end_found = 1;
                            }
                            break;
                        default:
                            break;
                    }

                    free(labelcontent);
                }
                break;
            default:
                break;
        }

        current_chunk += 8 + chunk_size;
    }

    if (loop_start_found && loop_end_found) *loop_flag = 1;

    /* labels don't seem to be consistently ordered */
    if (*loop_start > *loop_end) {
        long temp = *loop_start;
        *loop_start = *loop_end;
        *loop_end = temp;
    }
}

struct riff_fmt_chunk {
    int sample_rate;
    int channel_count;
    uint32_t block_size;
    int coding_type;
    int interleave;
};

int read_fmt(int big_endian,
    STREAMFILE * streamFile, 
    off_t current_chunk,
    struct riff_fmt_chunk * fmt,
    int sns,
    int mwv) {

    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    if (big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    fmt->sample_rate = read_32bit(current_chunk+0x0c,streamFile);
    fmt->channel_count = read_16bit(current_chunk+0x0a,streamFile);
    fmt->block_size = read_16bit(current_chunk+0x14,streamFile);

    switch ((uint16_t)read_16bit(current_chunk+0x8,streamFile)) {
        case 1: /* PCM */
            switch (read_16bit(current_chunk+0x16,streamFile)) {
                case 16:
                    if (big_endian) {
                        fmt->coding_type = coding_PCM16BE;
                    } else {
                        fmt->coding_type = coding_PCM16LE;
                    }
                    fmt->interleave = 2;
                    break;
                case 8:
                    fmt->coding_type = coding_PCM8_U_int;
                    fmt->interleave = 1;
                    break;
                default:
                    goto fail;
            }
            break;
        case 2: /* MS ADPCM */
            /* ensure 4bps */
            if (read_16bit(current_chunk+0x16,streamFile)!=4)
                goto fail;
            fmt->coding_type = coding_MSADPCM;
            fmt->interleave = 0;
            break;
        case 0x11:  /* MS IMA ADCM */
            /* ensure 4bps */
            if (read_16bit(current_chunk+0x16,streamFile)!=4)
                goto fail;
            fmt->coding_type = coding_MS_IMA;
            fmt->interleave = 0;
            break;
        case 0x69:  /* MS IMA ADCM - Rayman Raving Rabbids 2 (PC) */
            /* ensure 4bps */
            if (read_16bit(current_chunk+0x16,streamFile)!=4)
                goto fail;
            fmt->coding_type = coding_MS_IMA;
            fmt->interleave = 0;
            break;
        case 0x555: /* Level-5 0x555 ADPCM */
            if (!mwv) goto fail;
            fmt->coding_type = coding_L5_555;
            fmt->interleave = 0x12;
            break;
        case 0x5050: /* Ubisoft .sns uses this for DSP */
            if (!sns) goto fail;
            fmt->coding_type = coding_NGC_DSP;
            fmt->interleave = 8;
            break;
        case 0xFFF0: /* */
            fmt->coding_type = coding_NGC_DSP;
            fmt->interleave = 8;
            break;
        default:
            goto fail;
    }

    return 0;

fail:
    return -1;
}

VGMSTREAM * init_vgmstream_riff(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    struct riff_fmt_chunk fmt;

    off_t file_size = -1;
    int sample_count = 0;
    int fact_sample_count = -1;
    off_t start_offset = -1;

    int loop_flag = 0;
    long loop_start_ms = -1;
    long loop_end_ms = -1;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;
    uint32_t riff_size;
    uint32_t data_size = 0;

    int FormatChunkFound = 0;
    int DataChunkFound = 0;

    /* Level-5 mwv */
    int mwv = 0;
    off_t mwv_pflt_offset = -1;
    off_t mwv_ctrl_offset = -1;

    /* Ubisoft sns */
    int sns = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wav",filename_extension(filename)) &&
        strcasecmp("lwav",filename_extension(filename)))
    {
        if (!strcasecmp("mwv",filename_extension(filename)))
            mwv = 1;
        else if (!strcasecmp("sns",filename_extension(filename)))
            sns = 1;
        else
            goto fail;
    }

    /* check header */
    if ((uint32_t)read_32bitBE(0,streamFile)!=0x52494646) /* "RIFF" */
        goto fail;
    /* check for WAVE form */
    if ((uint32_t)read_32bitBE(8,streamFile)!=0x57415645) /* "WAVE" */
        goto fail;

    riff_size = read_32bitLE(4,streamFile);
    file_size = get_streamfile_size(streamFile);

    /* check for tructated RIFF */
    if (file_size < riff_size+8) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bitLE(current_chunk+4,streamFile);

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    if (-1 == read_fmt(0, /* big endian == false*/
                        streamFile,
                        current_chunk,
                        &fmt,
                        sns,
                        mwv))
                        goto fail;

                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
                case 0x4C495354:    /* LIST */
                    /* what lurks within?? */
                    switch (read_32bitBE(current_chunk + 8, streamFile)) {
                        case 0x6164746C:    /* adtl */
                            /* yay, atdl is its own little world */
                            parse_adtl(current_chunk + 8, chunk_size,
                                    streamFile,
                                    &loop_start_ms,&loop_end_ms,&loop_flag);
                            break;
                        default:
                            break;
                    }
                    break;
                case 0x736D706C:    /* smpl */
                    /* check loop count */
                    if (read_32bitLE(current_chunk+0x24, streamFile)==1)
                    {
                        /* check loop info */
                        if (read_32bitLE(current_chunk+0x2c+4, streamFile)==0)
                        {
                            loop_flag = 1;
                            loop_start_offset =
                                read_32bitLE(current_chunk+0x2c+8, streamFile);
                            loop_end_offset =
                                read_32bitLE(current_chunk+0x2c+0xc,streamFile);
                        }
                    }
                    break;
                case 0x70666c74:    /* pflt */
                    if (!mwv) break;    /* ignore if not in an mwv */
                    /* predictor filters */
                    mwv_pflt_offset = current_chunk;
                    break;
                case 0x6374726c:    /* ctrl */
                    if (!mwv) break;    /* ignore if not in an mwv */
                    /* loops! */
                    if (read_32bitLE(current_chunk+8, streamFile))
                    {
                        loop_flag = 1;
                    }
                    mwv_ctrl_offset = current_chunk;
                    break;
                case 0x66616374:    /* fact */
                    if (chunk_size != 4
                        && (!(sns && chunk_size == 0x10))) break;
                    fact_sample_count = read_32bitLE(current_chunk+8, streamFile);
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    switch (fmt.coding_type) {
        case coding_PCM16LE:
            sample_count = data_size/2/fmt.channel_count;
            break;
        case coding_PCM8_U_int:
            sample_count = data_size/fmt.channel_count;
            break;
        case coding_L5_555:
            sample_count = data_size/0x12/fmt.channel_count*32;
            break;
        case coding_MSADPCM:
            sample_count = msadpcm_bytes_to_samples(data_size, fmt.block_size, fmt.channel_count);
            break;
        case coding_MS_IMA:
            sample_count = (data_size / fmt.block_size) * (fmt.block_size - 4 * fmt.channel_count) * 2 / fmt.channel_count +
                ((data_size % fmt.block_size) ? (data_size % fmt.block_size - 4 * fmt.channel_count) * 2 / fmt.channel_count : 0);
            break;
        default:
            goto fail;
    }

    /* .sns uses fact chunk */
    if (sns)
    {
        if (-1 == fact_sample_count) goto fail;
        sample_count = fact_sample_count;
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(fmt.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = fmt.sample_rate;

    vgmstream->coding_type = fmt.coding_type;

    vgmstream->layout_type = layout_none;
    if (fmt.channel_count > 1) {
        switch (fmt.coding_type) {
            case coding_PCM8_U_int:
            case coding_MS_IMA:
            case coding_MSADPCM:
                // use layout_none from above
                break;
            default:
                vgmstream->layout_type = layout_interleave;
                break;
        }
    }

    vgmstream->interleave_block_size = fmt.interleave;
    switch (fmt.coding_type) {
        case coding_MSADPCM:
        case coding_MS_IMA:
            // override interleave_block_size with frame size
            vgmstream->interleave_block_size = fmt.block_size;
            break;
        default:
            // use interleave from above
            break;
    }

    if (loop_flag) {
        if (loop_start_ms >= 0)
        {
            vgmstream->loop_start_sample =
                (long long)loop_start_ms*fmt.sample_rate/1000;
            vgmstream->loop_end_sample =
                (long long)loop_end_ms*fmt.sample_rate/1000;
            vgmstream->meta_type = meta_RIFF_WAVE_labl_Marker;
        }
        else if (loop_start_offset >= 0)
        {
            vgmstream->loop_start_sample = loop_start_offset;
            vgmstream->loop_end_sample = loop_end_offset;
            vgmstream->meta_type = meta_RIFF_WAVE_smpl;
        }
        else if (mwv && mwv_ctrl_offset != -1)
        {
            vgmstream->loop_start_sample = read_32bitLE(mwv_ctrl_offset+12,
                    streamFile);
            vgmstream->loop_end_sample = sample_count;
        }
    }
    else
    {
        vgmstream->meta_type = meta_RIFF_WAVE;
    }

    if (mwv)
    {
        int i, c;
        if (fmt.coding_type == coding_L5_555)
        {
            const int filter_order = 3;
            int filter_count = read_32bitLE(mwv_pflt_offset+12, streamFile);

            if (mwv_pflt_offset == -1 ||
                    read_32bitLE(mwv_pflt_offset+8, streamFile) != filter_order ||
                    read_32bitLE(mwv_pflt_offset+4, streamFile) < 8 + filter_count * 4 * filter_order)
                goto fail;
            if (filter_count > 0x20) goto fail;
            for (c = 0; c < fmt.channel_count; c++)
            {
                for (i = 0; i < filter_count * filter_order; i++)
                {
                    vgmstream->ch[c].adpcm_coef_3by32[i] = read_32bitLE(
                            mwv_pflt_offset+16+i*4, streamFile
                            );
                }
            }
        }
        vgmstream->meta_type = meta_RIFF_WAVE_MWV;
    }

    if (sns)
    {
        int c;
        /* common codebook? */
        static const int16_t coef[16] =
        {0x04ab,0xfced,0x0789,0xfedf,0x09a2,0xfae5,0x0c90,0xfac1,
         0x084d,0xfaa4,0x0982,0xfdf7,0x0af6,0xfafa,0x0be6,0xfbf5};

        for (c = 0; c < fmt.channel_count; c++)
        {
            int i;
            for (i = 0; i < 16; i++)
            {
                vgmstream->ch[c].adpcm_coef[i] = coef[i];
            }
        }
        vgmstream->meta_type = meta_RIFF_WAVE_SNS;
    }

    /* open the file, set up each channel */
    {
        int i;

        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,
                STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!vgmstream->ch[0].streamfile) goto fail;

        for (i=0;i<fmt.channel_count;i++) {
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset =
                start_offset+i*fmt.interleave;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

VGMSTREAM * init_vgmstream_rifx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    struct riff_fmt_chunk fmt;

    off_t file_size = -1;
    int sample_count = 0;
    int fact_sample_count = -1;
    off_t start_offset = -1;
    off_t wiih_offset = -1;
    uint32_t wiih_size = 0;

    int loop_flag = 0;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;
    uint32_t riff_size;
    uint32_t data_size = 0;

    int FormatChunkFound = 0;
    int DataChunkFound = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wav",filename_extension(filename)) &&
        strcasecmp("lwav",filename_extension(filename)))
    {
        goto fail;
    }

    /* check header */
    if ((uint32_t)read_32bitBE(0,streamFile)!=0x52494658) /* "RIFX" */
        goto fail;
    /* check for WAVE form */
    if ((uint32_t)read_32bitBE(8,streamFile)!=0x57415645) /* "WAVE" */
        goto fail;

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
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    if (-1 == read_fmt(1, /* big endian == true */
                        streamFile,
                        current_chunk,
                        &fmt,
                        0,  /* sns == false */
                        0)) /* mwv == false */
                        goto fail;

                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
                case 0x736D706C:    /* smpl */
                    /* check loop count */
                    if (read_32bitBE(current_chunk+0x24, streamFile)==1)
                    {
                        /* check loop info */
                        if (read_32bitBE(current_chunk+0x2c+4, streamFile)==0)
                        {
                            loop_flag = 1;
                            loop_start_offset =
                                read_32bitBE(current_chunk+0x2c+8, streamFile);
                            loop_end_offset =
                                read_32bitBE(current_chunk+0x2c+0xc,streamFile);
                        }
                    }
                    break;
                case 0x66616374:    /* fact */
                    if (chunk_size != 4) break;
                    fact_sample_count = read_32bitBE(current_chunk+8, streamFile);
                    break;
                case 0x57696948:    /* WiiH */
                    wiih_size = read_32bitBE(current_chunk+4, streamFile);
                    wiih_offset = current_chunk+8;
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    switch (fmt.coding_type) {
        case coding_PCM16BE:
            sample_count = data_size/2/fmt.channel_count;
            break;
        case coding_PCM8_U_int:
            sample_count = data_size/fmt.channel_count;
            break;
        case coding_NGC_DSP:
            /* the only way of getting DSP info right now */
            if (wiih_offset < 0 || wiih_size != 0x2e*fmt.channel_count) goto fail;
            sample_count = data_size/8/fmt.channel_count*14;
            break;
#if 0
        /* found in RE:ORC, looks like it should be MS_IMA instead */
        case coding_MSADPCM:
            sample_count = msadpcm_bytes_to_samples(data_size, fmt.block_size, fmt.channel_count);
            break;
#endif
        default:
            goto fail;
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(fmt.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = fmt.sample_rate;

    vgmstream->coding_type = fmt.coding_type;

    vgmstream->layout_type = layout_none;
    if (fmt.channel_count > 1) {
        switch (fmt.coding_type) {
            case coding_PCM8_U_int:
            case coding_MS_IMA:
            case coding_MSADPCM:
                // use layout_none from above
                break;
            default:
                vgmstream->layout_type = layout_interleave;
                break;
        }
    }

    vgmstream->interleave_block_size = fmt.interleave;
    switch (fmt.coding_type) {
        case coding_MSADPCM:
        case coding_MS_IMA:
            // override interleave_block_size with frame size
            vgmstream->interleave_block_size = fmt.block_size;
            break;
        default:
            // use interleave from above
            break;
    }

    if (fmt.coding_type == coding_MS_IMA)
        vgmstream->interleave_block_size = fmt.block_size;

    if (loop_flag) {
        if (loop_start_offset >= 0)
        {
            vgmstream->loop_start_sample = loop_start_offset;
            vgmstream->loop_end_sample = loop_end_offset;
            vgmstream->meta_type = meta_RIFX_WAVE_smpl;
        }
    }
    else
    {
        vgmstream->meta_type = meta_RIFX_WAVE;
    }

    /* read from WiiH */
    if (wiih_offset >= 0) {
        int i,j;
        for (i=0;i<fmt.channel_count;i++) {
            for (j=0;j<16;j++)
                vgmstream->ch[i].adpcm_coef[j] = read_16bitBE(wiih_offset + i * 0x2e + j * 2,streamFile);
            vgmstream->ch[i].adpcm_history1_16 = read_16bitBE(wiih_offset + i * 0x2e + 0x24,streamFile);
            vgmstream->ch[i].adpcm_history2_16 = read_16bitBE(wiih_offset + i * 0x2e + 0x26,streamFile);
        }
    }

    /* open the file, set up each channel */
    {
        int i;

        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,
                STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!vgmstream->ch[0].streamfile) goto fail;

        for (i=0;i<fmt.channel_count;i++) {
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset =
                start_offset+i*fmt.interleave;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* XNBm (Windows 7 Phone) */
VGMSTREAM * init_vgmstream_xnbm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    struct riff_fmt_chunk fmt;

    off_t file_size = -1;
    int sample_count = 0;
    off_t start_offset = -1;

    int loop_flag = 0;
#if 0
    long loop_start_ms = -1;
    long loop_end_ms = -1;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;
#endif

    uint32_t xnbm_size;
    uint32_t data_size = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("xnb",filename_extension(filename)))
    {
        goto fail;
    }

    /* check header */
    if ((uint32_t)read_32bitBE(0,streamFile)!=0x584E426d) /* "XNBm" */
        goto fail;
    /* check version? */
    if ((uint32_t)read_16bitLE(4,streamFile)!=5)
        goto fail;

    xnbm_size = read_32bitLE(6,streamFile);
    file_size = get_streamfile_size(streamFile);

    /* check for tructated XNBm */
    if (file_size < xnbm_size) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xa; /* start with first chunk */
        int id_string_len;
        uint32_t fmt_chunk_size;

        /* flag? count of strings? */
        if (read_8bit(current_chunk ++, streamFile) != 1)
            goto fail;

        /* string length */
        id_string_len = read_8bit(current_chunk ++, streamFile);

        /* skip string */
        /* may want to check this ID "Microsoft.Xna.Framework.Content.SoundEffectReader" */
        current_chunk += id_string_len;

        /* ???? */
        if (read_32bitLE(current_chunk, streamFile) != 0)
            goto fail;
        current_chunk += 4;

        /* ???? */
        if (read_8bit(current_chunk ++, streamFile) != 0)
            goto fail;

        /* flag? count of chunks? */
        if (read_8bit(current_chunk++, streamFile) != 1)
            goto fail;

        /* fmt size */
        fmt_chunk_size = read_32bitLE(current_chunk, streamFile);
        current_chunk += 4;

        if (-1 == read_fmt(0, /* big endian == false */
                  streamFile,
                  current_chunk-8,  /* read_fmt() expects to skip "fmt "+size */
                  &fmt,
                  0,    /* sns == false */
                  0))   /* mwv == false */
                  goto fail;

        current_chunk += fmt_chunk_size;

        /* data size! */
        data_size = read_32bitLE(current_chunk, streamFile);
        current_chunk += 4;

        start_offset = current_chunk;
    }

    switch (fmt.coding_type) {
        case coding_PCM16LE:
            sample_count = data_size/2/fmt.channel_count;
            break;
        case coding_PCM8_U_int:
            sample_count = data_size/fmt.channel_count;
            break;
        case coding_MSADPCM:
            sample_count = msadpcm_bytes_to_samples(data_size, fmt.block_size, fmt.channel_count);
            break;
        case coding_MS_IMA:
            sample_count = (data_size / fmt.block_size) * (fmt.block_size - 4 * fmt.channel_count) * 2 / fmt.channel_count +
                ((data_size % fmt.block_size) ? (data_size % fmt.block_size - 4 * fmt.channel_count) * 2 / fmt.channel_count : 0);
            break;
        default:
            goto fail;
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(fmt.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = fmt.sample_rate;

    vgmstream->coding_type = fmt.coding_type;

    vgmstream->layout_type = layout_none;
    if (fmt.channel_count > 1) {
        switch (fmt.coding_type) {
            case coding_PCM8_U_int:
            case coding_MS_IMA:
            case coding_MSADPCM:
                // use layout_none from above
                break;
            default:
                vgmstream->layout_type = layout_interleave;
                break;
        }
    }

    vgmstream->interleave_block_size = fmt.interleave;
    switch (fmt.coding_type) {
        case coding_MSADPCM:
        case coding_MS_IMA:
            // override interleave_block_size with frame size
            vgmstream->interleave_block_size = fmt.block_size;
            break;
        default:
            // use interleave from above
            break;
    }

    vgmstream->meta_type = meta_XNBm;

    /* open the file, set up each channel */
    {
        int i;

        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,
                STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!vgmstream->ch[0].streamfile) goto fail;

        for (i=0;i<fmt.channel_count;i++) {
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset =
                start_offset+i*fmt.interleave;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

