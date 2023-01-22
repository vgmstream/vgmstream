#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"


/* known GENH types */
typedef enum {
    PSX = 0,            /* PS-ADPCM */
    XBOX = 1,           /* XBOX IMA ADPCM */
    NGC_DTK = 2,        /* NGC ADP/DTK ADPCM */
    PCM16BE = 3,        /* 16-bit big endian PCM */
    PCM16LE = 4,        /* 16-bit little endian PCM */
    PCM8 = 5,           /* 8-bit PCM */
    SDX2 = 6,           /* SDX2 (3D0 games) */
    DVI_IMA = 7,        /* DVI IMA ADPCM (high nibble first) */
    MPEG = 8,           /* MPEG (MP3) */
    IMA = 9,            /* IMA ADPCM (low nibble first) */
    AICA = 10,          /* YAMAHA AICA ADPCM (Dreamcast games) */
    MSADPCM = 11,       /* MS ADPCM (Windows games) */
    NGC_DSP = 12,       /* NGC DSP (Nintendo games) */
    PCM8_U_int = 13,    /* 8-bit unsigned PCM (interleaved) */
    PSX_bf = 14,        /* PS-ADPCM with bad flags */
    MS_IMA = 15,        /* Microsoft IMA ADPCM */
    PCM8_U = 16,        /* 8-bit unsigned PCM */
    APPLE_IMA4 = 17,    /* Apple Quicktime 4-bit IMA ADPCM */
    ATRAC3 = 18,        /* Raw ATRAC3 */
    ATRAC3PLUS = 19,    /* Raw ATRAC3PLUS */
    XMA1 = 20,          /* Raw XMA1 */
    XMA2 = 21,          /* Raw XMA2 */
    FFMPEG = 22,        /* Any headered FFmpeg format */
    AC3 = 23,           /* AC3/SPDIF */
    PCFX = 24,          /* PC-FX ADPCM */
    PCM4 = 25,          /* 4-bit signed PCM (3rd and 4th gen games) */
    PCM4_U = 26,        /* 4-bit unsigned PCM (3rd and 4th gen games) */
    OKI16 = 27,         /* OKI ADPCM with 16-bit output (unlike OKI/VOX/Dialogic ADPCM's 12-bit) */
    AAC = 28,           /* Advanced Audio Coding (raw without .mp4) */
} genh_type;

typedef struct {
    genh_type codec;
    int codec_mode;

    size_t interleave;
    size_t interleave_last;
    int channels;
    int32_t sample_rate;

    size_t data_size;
    off_t start_offset;

    int32_t num_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
    int skip_samples_mode;
    int32_t skip_samples;

    int loop_flag;

    int32_t coef_offset;
    int32_t coef_spacing;
    int32_t coef_split_offset;
    int32_t coef_split_spacing;
    int32_t coef_type;
    int32_t coef_interleave_type;
    int coef_big_endian;

} genh_header;

static int parse_genh(STREAMFILE * streamFile, genh_header * genh);

/* GENH is an artificial "generic" header for headerless streams */
VGMSTREAM* init_vgmstream_genh(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    genh_header genh = {0};
    coding_t coding;
    int i, j;


    /* checks */
    if (!is_id32be(0x0,sf, "GENH"))
        goto fail;
    if (!check_extensions(sf,"genh"))
        goto fail;

    /* process the header */
    if (!parse_genh(sf, &genh))
        goto fail;


    /* type to coding conversion */
    switch (genh.codec) {
        case PSX:        coding = coding_PSX; break;
        case XBOX:       coding = coding_XBOX_IMA; break;
        case NGC_DTK:    coding = coding_NGC_DTK; break;
        case PCM16BE:    coding = coding_PCM16BE; break;
        case PCM16LE:    coding = coding_PCM16LE; break;
        case PCM8:       coding = coding_PCM8; break;
        case SDX2:       coding = coding_SDX2; break;
        case DVI_IMA:    coding = coding_DVI_IMA; break;
#ifdef VGM_USE_MPEG
        case MPEG:       coding = coding_MPEG_layer3; break; /* we later find out exactly which */
#endif
        case IMA:        coding = coding_IMA; break;
        case AICA:       coding = coding_AICA; break;
        case MSADPCM:    coding = coding_MSADPCM; break;
        case NGC_DSP:    coding = coding_NGC_DSP; break;
        case PCM8_U_int: coding = coding_PCM8_U_int; break;
        case PSX_bf:     coding = coding_PSX_badflags; break;
        case MS_IMA:     coding = coding_MS_IMA; break;
        case PCM8_U:     coding = coding_PCM8_U; break;
        case APPLE_IMA4: coding = coding_APPLE_IMA4; break;
#ifdef VGM_USE_FFMPEG
        case ATRAC3:
        case ATRAC3PLUS:
        case XMA1:
        case XMA2:
        case AC3:
        case AAC:
        case FFMPEG:     coding = coding_FFmpeg; break;
#endif
        case PCFX:       coding = coding_PCFX; break;
        case PCM4:       coding = coding_PCM4; break;
        case PCM4_U:     coding = coding_PCM4_U; break;
        case OKI16:      coding = coding_OKI16; break;
        default:
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(genh.channels,genh.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = genh.sample_rate;
    vgmstream->num_samples = genh.num_samples;
    vgmstream->loop_start_sample = genh.loop_start_sample;
    vgmstream->loop_end_sample = genh.loop_end_sample;

    /* codec specific */
    switch (coding) {
        case coding_PCM8_U_int:
            vgmstream->layout_type = layout_none;
            break;
        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM8:
        case coding_PCM8_U:
        case coding_PCM4:
        case coding_PCM4_U:
        case coding_SDX2:
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_DVI_IMA:
        case coding_IMA:
        case coding_AICA:
        case coding_APPLE_IMA4:
            vgmstream->interleave_block_size = genh.interleave;
            vgmstream->interleave_last_block_size = genh.interleave_last;
            if (vgmstream->channels > 1)
            {
                if (coding == coding_SDX2) {
                    coding = coding_SDX2_int;
                }

                if (vgmstream->interleave_block_size==0xffffffff) {// || vgmstream->interleave_block_size == 0) {
                    vgmstream->layout_type = layout_none;
                }
                else {
                    vgmstream->layout_type = layout_interleave;
                    if (coding == coding_DVI_IMA)
                        coding = coding_DVI_IMA_int;
                    if (coding == coding_IMA)
                        coding = coding_IMA_int;
                    if (coding == coding_AICA)
                        coding = coding_AICA_int;
                }

                /* to avoid endless loops */
                if (!genh.interleave && (
                        coding == coding_PSX ||
                        coding == coding_PSX_badflags ||
                        coding == coding_IMA_int ||
                        coding == coding_DVI_IMA_int ||
                        coding == coding_SDX2_int) ) {
                    goto fail;
                }
            } else {
                vgmstream->layout_type = layout_none;
            }

            /* to avoid problems with dual stereo files (_L+_R) for codecs with stereo modes */
            if (coding == coding_AICA && genh.channels == 1)
                coding = coding_AICA_int;

            /* setup adpcm */
            if (coding == coding_AICA || coding == coding_AICA_int) {
                int ch;
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    vgmstream->ch[ch].adpcm_step_index = 0x7f;
                }
            }

            if (coding == coding_PCM4 || coding == coding_PCM4_U) {
                /* high nibble or low nibble first */
                vgmstream->codec_config = genh.codec_mode;
            }
            break;

        case coding_PCFX:
            vgmstream->interleave_block_size = genh.interleave;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_last_block_size = genh.interleave_last;
            if (genh.codec_mode >= 0 && genh.codec_mode <= 3)
                vgmstream->codec_config = genh.codec_mode;
            break;

        case coding_OKI16:
            vgmstream->layout_type = layout_none;
            break;

        case coding_MS_IMA:
            if (!genh.interleave) goto fail; /* creates garbage */

            vgmstream->interleave_block_size = genh.interleave;
            vgmstream->layout_type = layout_none;
            break;

        case coding_MSADPCM:
            if (vgmstream->channels > 2) goto fail;
            if (!genh.interleave) goto fail;

            vgmstream->frame_size = genh.interleave;
            vgmstream->layout_type = layout_none;
            break;

        case coding_XBOX_IMA:
            if (genh.codec_mode == 1) { /* mono interleave */
                coding = coding_XBOX_IMA_int;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_last_block_size = genh.interleave_last;
                vgmstream->interleave_block_size = genh.interleave;
            }
            else { /* 1ch mono, or stereo interleave */
                vgmstream->layout_type = genh.interleave ? layout_interleave : layout_none;
                vgmstream->interleave_block_size = genh.interleave;
                vgmstream->interleave_last_block_size = genh.interleave_last;
                if (vgmstream->channels > 2 && vgmstream->channels % 2 != 0)
                    goto fail; /* only 2ch+..+2ch layout is known */
            }
            break;
        case coding_NGC_DTK:
            if (vgmstream->channels != 2) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        case coding_NGC_DSP:
            if (genh.coef_interleave_type == 0) {
                if (!genh.interleave) goto fail;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = genh.interleave;
                vgmstream->interleave_last_block_size = genh.interleave_last;
            } else if (genh.coef_interleave_type == 1) {
                if (!genh.interleave) goto fail;
                coding = coding_NGC_DSP_subint;
                vgmstream->interleave_block_size = genh.interleave;
                vgmstream->layout_type = layout_none;
            } else if (genh.coef_interleave_type == 2) {
                vgmstream->layout_type = layout_none;
            }// else {
            //   goto fail;
            //}

            /* get coefs */
            for (i=0;i<vgmstream->channels;i++) {
                int16_t (*read_16bit)(off_t , STREAMFILE*) = genh.coef_big_endian ? read_16bitBE : read_16bitLE;

                /* normal/split coefs */
                if ((genh.coef_type & 1) == 0) { /* normal mode */
                    for (j = 0; j < 16; j++) {
                        vgmstream->ch[i].adpcm_coef[j] = read_16bit(genh.coef_offset + i*genh.coef_spacing + j*2, sf);
                    }
                }
                else { /* split coefs, 8 coefs in the main array, additional offset to 2nd array given at 0x34 for left, 0x38 for right */
                    for (j = 0; j < 8; j++) {
                        vgmstream->ch[i].adpcm_coef[j*2] = read_16bit(genh.coef_offset + i*genh.coef_spacing + j*2, sf);
                        vgmstream->ch[i].adpcm_coef[j*2+1] = read_16bit(genh.coef_split_offset + i*genh.coef_split_spacing + j*2, sf);
                    }
                }
            }

            break;
#ifdef VGM_USE_MPEG
        case coding_MPEG_layer3:
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_mpeg(sf, genh.start_offset, &coding, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *ffmpeg_data = NULL;

            if (genh.codec == FFMPEG || genh.codec == AC3 || genh.codec == AAC) {
                /* default FFmpeg */
                ffmpeg_data = init_ffmpeg_offset(sf, genh.start_offset,genh.data_size);
                if ( !ffmpeg_data ) goto fail;

                //if (vgmstream->num_samples == 0)
                //    vgmstream->num_samples = ffmpeg_get_samples(ffmpeg_data); /* sometimes works */
            }
            else if (genh.codec == ATRAC3) {
                int block_align, encoder_delay;

                block_align = genh.interleave;
                encoder_delay = genh.skip_samples;

                ffmpeg_data = init_ffmpeg_atrac3_raw(sf, genh.start_offset,genh.data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
                if (!ffmpeg_data) goto fail;
            }
            else if (genh.codec == ATRAC3PLUS) {
                int block_size = genh.interleave;

                ffmpeg_data = init_ffmpeg_atrac3plus_raw(sf, genh.start_offset, genh.data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, genh.skip_samples);
                if (!ffmpeg_data) goto fail;
            }
            else if (genh.codec == XMA1) {
                int xma_stream_mode = genh.codec_mode == 1 ? 1 : 0;

                ffmpeg_data = init_ffmpeg_xma1_raw(sf, genh.start_offset, genh.data_size, vgmstream->channels, vgmstream->sample_rate, xma_stream_mode);
                if (!ffmpeg_data) goto fail;
            }
            else if (genh.codec == XMA2) {
                int block_size = genh.interleave;

                ffmpeg_data = init_ffmpeg_xma2_raw(sf, genh.start_offset, genh.data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, 0);
                if (!ffmpeg_data) goto fail;
            }
            else {
                goto fail;
            }

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->layout_type = layout_none;

            if (genh.codec == XMA1 || genh.codec == XMA2) {
                xma_fix_raw_samples(vgmstream, sf, genh.start_offset,genh.data_size, 0, 0,0);
            } else if (genh.skip_samples_mode && genh.skip_samples >= 0 && genh.codec != ATRAC3) { /* force encoder delay */
                ffmpeg_set_skip_samples(ffmpeg_data, genh.skip_samples);
            }

            break;
        }
#endif
        default:
            break;
    }

    vgmstream->coding_type = coding;
    vgmstream->meta_type = meta_GENH;
    vgmstream->allow_dual_stereo = 1;


    if ( !vgmstream_open_stream(vgmstream,sf,genh.start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_genh(STREAMFILE * streamFile, genh_header * genh) {
    size_t header_size;

    genh->channels = read_32bitLE(0x04,streamFile);

    genh->interleave = read_32bitLE(0x08,streamFile);
    genh->sample_rate = read_32bitLE(0x0c,streamFile);
    genh->loop_start_sample = read_32bitLE(0x10,streamFile);
    genh->loop_end_sample = read_32bitLE(0x14,streamFile);

    genh->codec = read_32bitLE(0x18,streamFile);
    genh->start_offset = read_32bitLE(0x1C,streamFile);
    header_size = read_32bitLE(0x20,streamFile);
    /* HACK to support old genh */
    if (header_size == 0) {
        genh->start_offset = 0x800;
        header_size = 0x800;
    }

    if (header_size > genh->start_offset) /* audio data start past header end */
        goto fail;
    if (header_size < 0x24) /* absolute minimum for GENH */
        goto fail;

    /* DSP coefficients */
    if (header_size >= 0x30) {
        genh->coef_offset = read_32bitLE(0x24,streamFile);
        if (genh->channels == 2) /* old meaning, "coef right offset" */
            genh->coef_spacing = read_32bitLE(0x28,streamFile) - genh->coef_offset;
        else if (genh->channels > 2) /* new meaning, "coef spacing" */
            genh->coef_spacing = read_32bitLE(0x28,streamFile);
        genh->coef_interleave_type = read_32bitLE(0x2C,streamFile);
    }

    /* DSP coefficient variants */
    if (header_size >= 0x34) {
        /* bit 0 flag - split coefs (2 arrays) */
        /* bit 1 flag - little endian coefs (for some 3DS) */
        genh->coef_type = read_32bitLE(0x30,streamFile);
        genh->coef_big_endian = ((genh->coef_type & 2) == 0);
    }

    /* DSP split coefficients' 2nd array */
    if (header_size >= 0x3c) {
        genh->coef_split_offset = read_32bitLE(0x34,streamFile);
        if (genh->channels == 2) /* old meaning, "coef right offset" */
            genh->coef_split_spacing = read_32bitLE(0x38,streamFile) - genh->coef_split_offset;
        else if (genh->channels > 2) /* new meaning, "coef spacing" */
            genh->coef_split_spacing = read_32bitLE(0x38,streamFile);
    }

    /* extended + reserved fields */
    if (header_size >= 0x100) {
        genh->num_samples = read_32bitLE(0x40,streamFile);
        genh->skip_samples = read_32bitLE(0x44,streamFile); /* for FFmpeg based codecs */
        genh->skip_samples_mode = read_8bit(0x48,streamFile); /* 0=autodetect, 1=force manual value @ 0x44 */
        genh->codec_mode = read_8bit(0x4b,streamFile);
        if ((genh->codec == ATRAC3 || genh->codec == ATRAC3PLUS) && genh->codec_mode==0)
            genh->codec_mode = read_8bit(0x49,streamFile);
        if ((genh->codec == XMA1 || genh->codec == XMA2) && genh->codec_mode==0)
            genh->codec_mode = read_8bit(0x4a,streamFile);
        genh->data_size = read_32bitLE(0x50,streamFile);
        genh->interleave_last = read_32bitLE(0x54,streamFile);
    }

    if (genh->data_size == 0)
        genh->data_size = get_streamfile_size(streamFile) - genh->start_offset;
    genh->num_samples = genh->num_samples > 0 ? genh->num_samples : genh->loop_end_sample;
    genh->loop_flag = genh->loop_start_sample != -1;

    /* fix for buggy GENHs that used to work before interleaved XBOX-IMA was added
     * (could do check for other cases, but maybe it's better to give bad sound on nonsense values) */
    if (genh->codec == XBOX && genh->interleave < 0x24) { /* found as 0x2 */
        genh->interleave = 0;
    }

    return 1;

fail:
    return 0;
}
