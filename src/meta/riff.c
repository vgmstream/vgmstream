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
    off_t offset;
    off_t size;
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

    fmt->offset = current_chunk;
    fmt->size = read_32bit(current_chunk+0x4,streamFile);

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
#ifdef VGM_USE_FFMPEG
		case 0x270: /* ATRAC3 */
#if defined(VGM_USE_FFMPEG) && !defined(VGM_USE_MAIATRAC3PLUS)
        case 0xFFFE: /* WAVEFORMATEXTENSIBLE / ATRAC3plus */
#endif /* defined */
			fmt->coding_type = coding_FFmpeg;
			fmt->interleave = 0;
			break;
#endif /* VGM_USE_FFMPEG */
#ifdef VGM_USE_MAIATRAC3PLUS
		case 0xFFFE: /* WAVEFORMATEXTENSIBLE / ATRAC3plus */
			if (read_32bit(current_chunk+0x20,streamFile) == 0xE923AABF &&
				read_16bit(current_chunk+0x24,streamFile) == (int16_t)0xCB58 &&
				read_16bit(current_chunk+0x26,streamFile) == 0x4471 &&
				read_32bitLE(current_chunk+0x28,streamFile) == 0xFAFF19A1 &&
				read_32bitLE(current_chunk+0x2C,streamFile) == 0x62CEE401) {
				uint16_t bztmp = read_16bit(current_chunk+0x32,streamFile);
				bztmp = (bztmp >> 8) | (bztmp << 8);
				fmt->coding_type = coding_AT3plus;
				fmt->block_size = (bztmp & 0x3FF) * 8 + 8;
				fmt->interleave = 0;
			}
			break;
#endif
        default:
            goto fail;
    }

    return 0;

fail:
    return -1;
}

VGMSTREAM * init_vgmstream_riff(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    struct riff_fmt_chunk fmt;

#ifdef VGM_USE_FFMPEG
    ffmpeg_codec_data *ffmpeg_data = NULL;
#endif

    off_t file_size = -1;
    int sample_count = 0;
    int fact_sample_count = -1;
    int fact_sample_skip = -1;
    off_t start_offset = -1;

    int loop_flag = 0;
    long loop_start_ms = -1;
    long loop_end_ms = -1;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;
    uint32_t riff_size;
    uint32_t data_size = 0;

    int FormatChunkFound = 0, DataChunkFound = 0, JunkFound = 0;

    /* Level-5 mwv */
    int mwv = 0;
    off_t mwv_pflt_offset = -1;
    off_t mwv_ctrl_offset = -1;

    /* Ubisoft sns */
    int sns = 0;

	/* Sony atrac3 / 3plus */
    int at3 = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wav",filename_extension(filename)) &&
        strcasecmp("lwav",filename_extension(filename))
#ifndef VGM_USE_FFMPEG
        && strcasecmp("sgb",filename_extension(filename)) /* SGB has proper support with FFmpeg in sgxd */
#endif
		)
    {
        if (!strcasecmp("mwv",filename_extension(filename)))
            mwv = 1;
        else if (!strcasecmp("sns",filename_extension(filename)))
            sns = 1;
#if defined(VGM_USE_MAIATRAC3PLUS) || defined(VGM_USE_FFMPEG)
        else if ( check_extensions(streamFile, "at3,rws") ) /* Renamed .RWS AT3 found in Climax games (Silent Hill Origins PSP, Oblivion PSP) */
            at3 = 1;
#endif
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
                    if (sns && chunk_size == 0x10) {
                        fact_sample_count = read_32bitLE(current_chunk+0x8, streamFile);
                    } else if (at3 && chunk_size == 0x8) {
                        fact_sample_count = read_32bitLE(current_chunk+0x8, streamFile);
                        fact_sample_skip  = read_32bitLE(current_chunk+0xc, streamFile);
                    } else if (at3 && chunk_size == 0xc) {
                        fact_sample_count = read_32bitLE(current_chunk+0x8, streamFile);
                        fact_sample_skip  = read_32bitLE(current_chunk+0x10, streamFile);
                    }

                    break;
                case 0x4A554E4B:    /* JUNK */
                    JunkFound = 1;
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    /* JUNK is an optional Wwise chunk, and Wwise hijacks the MSADPCM/MS_IMA/XBOX IMA ids (how nice).
     * To ensure their stuff is parsed in wwise.c we reject their JUNK, which they put almost always.
     * As JUNK is legal (if unusual) we only reject those codecs.
     * (ex. Cave PC games have PCM16LE + JUNK + smpl created by "Samplitude software") */
    if (JunkFound && (fmt.coding_type==coding_MSADPCM || fmt.coding_type==coding_MS_IMA)) goto fail;

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
        case coding_NGC_DSP:
            //sample_count = data_size / fmt.channel_count / 8 * 14; /* expected from the "fact" chunk */
            break;
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            {
                ffmpeg_data = init_ffmpeg_offset(streamFile, 0, streamFile->get_size(streamFile) );
                if ( !ffmpeg_data ) goto fail;

                sample_count = ffmpeg_data->totalSamples; /* fact_sample_count */

                if (at3) {
                    /* the encoder introduces some garbage (not always silent) samples to skip before the stream */
                    /* manually set skip_samples if FFmpeg didn't do it */
                    if (ffmpeg_data->skipSamples <= 0) {
                        ffmpeg_set_skip_samples(ffmpeg_data, fact_sample_skip);
                    }

                    /* RIFF loop/sample values are absolute (with skip samples), adjust */
                    if (loop_flag) {
                        loop_start_offset -= ffmpeg_data->skipSamples;
                        loop_end_offset -= ffmpeg_data->skipSamples;
                    }
                }
            }
            break;
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
		case coding_AT3plus:
            /* rough total samples (block_size may be incorrect if not using joint stereo) */
            sample_count = (data_size / fmt.block_size) * 2048;
            /* favor fact_samples if available (skip isn't correctly handled for now) */
            if (fact_sample_count > 0 && fact_sample_count + fact_sample_skip < sample_count)
                sample_count = fact_sample_count + fact_sample_skip;

			break;
#endif
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
#ifdef VGM_USE_FFMPEG
            case coding_FFmpeg:
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
			case coding_AT3plus:
#endif
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
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
		case coding_AT3plus:
#endif
            // override interleave_block_size with frame size
            vgmstream->interleave_block_size = fmt.block_size;
            break;
        default:
            // use interleave from above
            break;
    }

#ifdef VGM_USE_FFMPEG
    if (fmt.coding_type == coding_FFmpeg) {
        vgmstream->codec_data = ffmpeg_data;
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
	if (fmt.coding_type == coding_AT3plus) {
		maiatrac3plus_codec_data *data = malloc(sizeof(maiatrac3plus_codec_data));
		data->buffer = 0;
		data->samples_discard = 0;
		data->handle = Atrac3plusDecoder_openContext();
		vgmstream->codec_data = data;
	}
#endif

    if (loop_flag) {
        if (loop_start_ms >= 0)
        {
            vgmstream->loop_start_sample =
                (long long)loop_start_ms*fmt.sample_rate/1000;
            vgmstream->loop_end_sample =
                (long long)loop_end_ms*fmt.sample_rate/1000;
            vgmstream->meta_type = meta_RIFF_WAVE_labl;
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
#ifdef VGM_USE_FFMPEG
    if (ffmpeg_data) {
        free_ffmpeg(ffmpeg_data);
        if (vgmstream) vgmstream->codec_data = NULL;
    }
#endif
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

VGMSTREAM * init_vgmstream_rifx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    struct riff_fmt_chunk fmt;

    off_t file_size = -1;
    int sample_count = 0;
    //int fact_sample_count = -1;
    off_t start_offset = -1;

    int loop_flag = 0;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;
    uint32_t riff_size;
    uint32_t data_size = 0;

    int FormatChunkFound = 0, DataChunkFound = 0, JunkFound = 0;

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
                    //fact_sample_count = read_32bitBE(current_chunk+8, streamFile);
                    break;
                case 0x4A554E4B:    /* JUNK */
                    JunkFound = 1;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    /* JUNK is an optional Wwise chunk, and Wwise hijacks the MSADPCM/MS_IMA/XBOX IMA ids (how nice).
     * To ensure their stuff is parsed in wwise.c we reject their JUNK, which they put almost always.
     * As JUNK is legal (if unusual) we only reject those codecs.
     * (ex. Cave PC games have PCM16LE + JUNK + smpl created by "Samplitude software") */
    if (JunkFound && (fmt.coding_type==coding_MSADPCM || fmt.coding_type==coding_MS_IMA)) goto fail;

    switch (fmt.coding_type) {
        case coding_PCM16BE:
            sample_count = data_size/2/fmt.channel_count;
            break;
        case coding_PCM8_U_int:
            sample_count = data_size/fmt.channel_count;
            break;
        case coding_NGC_DSP:
            //sample_count = data_size / fmt.channel_count / 8 * 14; /* expected from the "fact" chunk */
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
    char filename[PATH_LIMIT];

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

