#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"

/* known GENH types */
typedef enum {
    PSX = 0,          /* PSX ADPCM */
    XBOX = 1,         /* XBOX IMA ADPCM */
    NGC_DTK = 2,      /* NGC ADP/DTK ADPCM */
    PCM16BE = 3,      /* 16bit big endian PCM */
    PCM16LE = 4,      /* 16bit little endian PCM */
    PCM8 = 5,         /* 8bit PCM */
    SDX2 = 6,         /* SDX2 (3D0 games) */
    DVI_IMA = 7,      /* DVI IMA ADPCM */
    MPEG = 8,         /* MPEG (MP3) */
    IMA = 9,          /* IMA ADPCM */
    AICA = 10,        /* AICA ADPCM (dreamcast) */
    MSADPCM = 11,     /* MS ADPCM (windows) */
    NGC_DSP = 12,     /* NGC DSP (GC) */
    PCM8_U_int = 13,  /* 8bit unsigned PCM (interleaved) */
    PSX_bf = 14,      /* PSX ADPCM bad flagged */
    MS_IMA = 15,      /* Microsoft IMA ADPCM */
    PCM8_U = 16,      /* 8bit unsigned PCM */
    APPLE_IMA4 = 17,  /* Apple Quicktime 4-bit IMA ADPCM */
    ATRAC3 = 18,      /* raw ATRAC3 */
    ATRAC3PLUS = 19,  /* raw ATRAC3PLUS */
    XMA1 = 20,        /* raw XMA1 */
    XMA2 = 21,        /* raw XMA2 */
    FFMPEG = 22,      /* any headered FFmpeg format */
} genh_type;

/* GENH is an artificial "generic" header for headerless streams */
VGMSTREAM * init_vgmstream_genh(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    
    int channel_count, loop_flag, sample_rate, interleave;
    int32_t num_samples = 0, loop_start, loop_end, skip_samples = 0;
    int32_t start_offset, header_size;
    off_t datasize = 0;

    int32_t coef[2];
    int32_t coef_splitted[2];
    int32_t dsp_interleave_type;
    int32_t coef_type;
    int skip_samples_mode, atrac3_mode, xma_mode;
    int i, j;

    coding_t coding;
    genh_type type;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"genh")) goto fail;

    /* check header magic */
    if (read_32bitBE(0x0,streamFile) != 0x47454e48) goto fail;

    channel_count = read_32bitLE(0x4,streamFile);
    if (channel_count < 1) goto fail;

    type = read_32bitLE(0x18,streamFile);
    /* type to coding conversion */
    switch (type) {
        case PSX:        coding = coding_PSX; break;
        case XBOX:       coding = coding_XBOX; break;
        case NGC_DTK:    coding = coding_NGC_DTK; break;
        case PCM16BE:    coding = coding_PCM16BE; break;
        case PCM16LE:    coding = coding_PCM16LE; break;
        case PCM8:       coding = coding_PCM8; break;
        case SDX2:       coding = coding_SDX2; break;
        case DVI_IMA:    coding = coding_DVI_IMA; break;
#ifdef VGM_USE_MPEG
        case MPEG:       coding = coding_MPEG1_L3; break; /* we later find out exactly which */
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
        case FFMPEG:     coding = coding_FFmpeg; break;
#endif
        default:
            goto fail;
    }

    start_offset = read_32bitLE(0x1C,streamFile);
    header_size = read_32bitLE(0x20,streamFile);

    /* HACK to support old genh */
    if (header_size == 0) {
        start_offset = 0x800;
        header_size = 0x800;
    }

    /* check for audio data start past header end */
    if (header_size > start_offset) goto fail;

    interleave = read_32bitLE(0x8,streamFile);
    sample_rate = read_32bitLE(0xc,streamFile);
    loop_start = read_32bitLE(0x10,streamFile);
    loop_end = read_32bitLE(0x14,streamFile);
    
    coef[0] = read_32bitLE(0x24,streamFile);
    coef[1] = read_32bitLE(0x28,streamFile);
    dsp_interleave_type = read_32bitLE(0x2C,streamFile);

    /* DSP coefficient variants */
    /* bit 0 - split coefs (2 arrays) */
    /* bit 1 - little endian coefs */
    coef_type = read_32bitLE(0x30,streamFile); 
    /* when using split coefficients, 2nd array is at: */
    coef_splitted[0] = read_32bitLE(0x34,streamFile);
    coef_splitted[1] = read_32bitLE(0x38,streamFile);

    /* other fields */
    num_samples = read_32bitLE(0x40,streamFile);
    skip_samples = read_32bitLE(0x44,streamFile); /* for FFmpeg based codecs */
    skip_samples_mode = read_8bit(0x48,streamFile); /* 0=autodetect, 1=force manual value @ 0x44 */
    atrac3_mode = read_8bit(0x49,streamFile); /* 0=autodetect, 1=force joint stereo, 2=force full stereo */
    xma_mode = read_8bit(0x4a,streamFile); /* 0=default (4ch = 2ch + 2ch), 1=single (4ch = 1ch + 1ch + 1ch + 1ch) */
    datasize = read_32bitLE(0x50,streamFile);
    if (!datasize)
        datasize = get_streamfile_size(streamFile)-start_offset;

    num_samples = num_samples > 0 ? num_samples : loop_end;
    loop_flag = loop_start != -1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    /* codec specific */
    switch (coding) {
        case coding_PCM8_U_int:
            vgmstream->layout_type=layout_none;
            break;
        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM8:
        case coding_PCM8_U:
        case coding_SDX2:
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_DVI_IMA:
        case coding_IMA:
        case coding_AICA:
        case coding_APPLE_IMA4:
            vgmstream->interleave_block_size = interleave;
            if (channel_count > 1)
            {
                if (coding == coding_SDX2) {
                    coding = coding_SDX2_int;
                }

                if (vgmstream->interleave_block_size==0xffffffff) {
                    vgmstream->layout_type = layout_none;
                }
                else {
                    vgmstream->layout_type = layout_interleave;
                    if (coding == coding_DVI_IMA)
                        coding = coding_DVI_IMA_int;
                    if (coding == coding_IMA)
                        coding = coding_IMA_int;
                }

                /* to avoid endless loops */
                if (!interleave && (
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

            /* setup adpcm */
            if (coding == coding_AICA) {
                int i;
                for (i=0;i<channel_count;i++) {
                    vgmstream->ch[i].adpcm_step_index = 0x7f;
                }
            }

            break;
        case coding_MS_IMA:
            if (!interleave) goto fail; /* creates garbage */

            vgmstream->interleave_block_size = interleave;
            vgmstream->layout_type = layout_none;
            break;
        case coding_MSADPCM:
            if (channel_count > 2) goto fail;
            if (!interleave) goto fail; /* creates garbage */

            vgmstream->interleave_block_size = interleave;
            vgmstream->layout_type = layout_none;
            break;
        case coding_XBOX:
            vgmstream->layout_type = layout_none;
            break;
        case coding_NGC_DTK:
            if (channel_count != 2) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        case coding_NGC_DSP:
            if (dsp_interleave_type == 0) {
                if (!interleave) goto fail;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = interleave;
            } else if (dsp_interleave_type == 1) {
                if (!interleave) goto fail;
                vgmstream->layout_type = layout_interleave_byte;
                vgmstream->interleave_block_size = interleave;
            } else if (dsp_interleave_type == 2) {
                vgmstream->layout_type = layout_none;
            }

            /* get coefs */
            for (i=0;i<channel_count;i++) {
                int16_t (*read_16bit)(off_t , STREAMFILE*);
                /* bit 1 - little endian coefs */
                if ((coef_type & 2) == 0) {
                    read_16bit = read_16bitBE;
                } else {
                    read_16bit = read_16bitLE;
                }

                /* bit 0 - split coefs (2 arrays) */
                if ((coef_type & 1) == 0) {
                    for (j=0;j<16;j++) {
                        vgmstream->ch[i].adpcm_coef[j] = read_16bit(coef[i]+j*2,streamFile);
                    }
                } else {
                    for (j=0;j<8;j++) {
                        vgmstream->ch[i].adpcm_coef[j*2]=read_16bit(coef[i]+j*2,streamFile);
                        vgmstream->ch[i].adpcm_coef[j*2+1]=read_16bit(coef_splitted[i]+j*2,streamFile);
                    }
                }
            }

            break;
#ifdef VGM_USE_MPEG
        case coding_MPEG1_L3:
            vgmstream->layout_type = layout_mpeg;
            vgmstream->codec_data = init_mpeg_codec_data(streamFile, start_offset, &coding, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *ffmpeg_data = NULL;

            if (type == FFMPEG) {
                /* default FFmpeg */
                ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,datasize);
                if ( !ffmpeg_data ) goto fail;
            }
            else {
                /* fake header FFmpeg */
                uint8_t buf[200];
                int32_t bytes;

                if (type == ATRAC3) {
                    int block_size = interleave;
                    int joint_stereo;
                    switch(atrac3_mode) {
                        case 0: joint_stereo = vgmstream->channels > 1 && interleave/vgmstream->channels==0x60 ? 1 : 0; break; /* autodetect */
                        case 1: joint_stereo = 1; break; /* force joint stereo */
                        case 2: joint_stereo = 0; break; /* force stereo */
                        default: goto fail;
                    }

                    bytes = ffmpeg_make_riff_atrac3(buf, 200, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, skip_samples);
                }
                else if (type == ATRAC3PLUS) {
                    int block_size = interleave;

                    bytes = ffmpeg_make_riff_atrac3plus(buf, 200, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_size, skip_samples);
                }
                else if (type == XMA1) {
                    int xma_stream_mode = xma_mode == 1 ? 1 : 0;

                    bytes = ffmpeg_make_riff_xma1(buf, 100, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, xma_stream_mode);
                }
                else if (type == XMA2) {
                    int block_size = interleave ? interleave : 2048;
                    int block_count = datasize / block_size;

                    bytes = ffmpeg_make_riff_xma2(buf, 200, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
                }
                else {
                    goto fail;
                }
                if (bytes <= 0) goto fail;

                ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
                if ( !ffmpeg_data ) goto fail;
            }

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->layout_type = layout_none;

            /* force encoder delay */
            if (skip_samples_mode && skip_samples >= 0) {
                ffmpeg_set_skip_samples(ffmpeg_data, skip_samples);
            }

            break;
        }
#endif
        default:
            break;
    }

    vgmstream->coding_type = coding;
    vgmstream->meta_type = meta_GENH;


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
