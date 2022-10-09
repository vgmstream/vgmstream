#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "txth_streamfile.h"
#include "../util/text_reader.h"
#include "../util/endianness.h"

#define TXT_LINE_MAX 2048 /* probably ~1000 would be ok */
#define TXT_LINE_KEY_MAX 128
#define TXT_LINE_VAL_MAX (TXT_LINE_MAX - TXT_LINE_KEY_MAX)

/* known TXTH types */
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
    TGC = 29,           /* Tiger Game.com 4-bit ADPCM */
    ASF = 30,           /* Argonaut ASF 4-bit ADPCM */
    EAXA = 31,          /* Electronic Arts EA-XA 4-bit ADPCM v1 */
    OKI4S = 32,         /* OKI ADPCM with 16-bit output (unlike OKI/VOX/Dialogic ADPCM's 12-bit) */
    XA,
    XA_EA,
    CP_YM,
    PCM_FLOAT_LE,
    IMA_HV,
    PCM8_SB,
    HEVAG,
    YMZ,

    UNKNOWN = 99,
} txth_codec_t;

typedef enum { DEFAULT, NEGATIVE, POSITIVE, INVERTED } txth_loop_t;

typedef struct {
    txth_codec_t codec;
    uint32_t codec_mode;

    uint32_t value_mul;
    uint32_t value_div;
    uint32_t value_add;
    uint32_t value_sub;

    uint32_t id_value;
    uint32_t id_check;

    uint32_t interleave;
    uint32_t interleave_last;
    uint32_t interleave_first;
    uint32_t interleave_first_skip;
    uint32_t channels;
    uint32_t sample_rate;

    uint32_t data_size;
    int data_size_set;
    uint32_t start_offset;
    uint32_t next_offset;
    uint32_t padding_size;

    int sample_type;
    uint32_t num_samples;
    uint32_t loop_start_sample;
    uint32_t loop_end_sample;
    uint32_t loop_adjust;
    int skip_samples_set;
    uint32_t skip_samples;

    uint32_t loop_flag;
    txth_loop_t loop_behavior;
    int loop_flag_set;
    int loop_flag_auto;

    uint32_t coef_offset;
    uint32_t coef_spacing;
    uint32_t coef_big_endian;
    uint32_t coef_mode;
    int coef_table_set;
    uint8_t coef_table[0x02*16 * 16]; /* reasonable max */

    int hist_set;
    uint32_t hist_offset;
    uint32_t hist_spacing;
    uint32_t hist_big_endian;

    int num_samples_data_size;

    int target_subsong;
    uint32_t subsong_count;
    uint32_t subsong_spacing;

    uint32_t name_offset_set;
    uint32_t name_offset;
    uint32_t name_size;

    int subfile_set;
    uint32_t subfile_offset;
    uint32_t subfile_size;
    char subfile_extension[32];

    uint32_t chunk_number;
    uint32_t chunk_start;
    uint32_t chunk_size;
    uint32_t chunk_count;
    uint32_t chunk_header_size;
    uint32_t chunk_data_size;
    uint32_t chunk_value;
    uint32_t chunk_size_offset;
    uint32_t chunk_be;
    int chunk_start_set;
    int chunk_size_set;
    int chunk_count_set;

    uint32_t base_offset;
    uint32_t is_offset_absolute;

    uint32_t name_values[16];
    int name_values_count;

    int is_multi_txth;

    /* original STREAMFILE and its type (may be an unsupported "base" file or a .txth) */
    STREAMFILE* sf;
    int streamfile_is_txth;

    /* configurable STREAMFILEs and if we opened it (thus must close it later) */
    STREAMFILE* sf_text;
    STREAMFILE* sf_head;
    STREAMFILE* sf_body;
    int sf_text_opened;
    int sf_head_opened;
    int sf_body_opened;

    int debug;

} txth_header;

static VGMSTREAM* init_subfile(txth_header* txth);
static STREAMFILE* open_txth(STREAMFILE* sf);
static void clean_txth(txth_header* txth);
static int parse_txth(txth_header* txth);


/* TXTH - an artificial "generic" header for headerless streams.
 * Similar to GENH, but with a single separate .txth file in the dir and text-based. */
VGMSTREAM* init_vgmstream_txth(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    txth_header txth = {0};
    coding_t coding;
    int i, j;


    /* accept .txth (should set body_file or will fail later) */
    if (check_extensions(sf, "txth")) {
        txth.sf = sf;
        txth.streamfile_is_txth = 1;

        txth.sf_text = sf;
        txth.sf_head = NULL;
        txth.sf_body = NULL;
        txth.sf_text_opened = 0;
        txth.sf_head_opened = 0;
        txth.sf_body_opened = 0;
    }
    else {
        /* accept base file (no need for ID or ext checks --if a companion .TXTH exists all is good).
         * player still needs to accept the streamfile's ext, so at worst rename to .vgmstream */
        STREAMFILE* sf_text = open_txth(sf);
        if (!sf_text) goto fail;

        txth.sf = sf;
        txth.streamfile_is_txth = 0;

        txth.sf_text = sf_text;
        txth.sf_head = sf;
        txth.sf_body = sf;
        txth.sf_text_opened = 1;
        txth.sf_head_opened = 0;
        txth.sf_body_opened = 0;
    }

    /* process the text file */
    if (!parse_txth(&txth))
        goto fail;

    /* special case of parsing subfiles */
    if (txth.subfile_set) {
        VGMSTREAM* subfile_vgmstream = init_subfile(&txth);
        clean_txth(&txth);
        return subfile_vgmstream;
    }


    /* set common interleaves to simplify usage
        * (maybe should ignore if manually overwritten, possibly with 0 on purpose?) */
    if (txth.interleave == 0) {
        uint32_t interleave  = 0;
        switch(txth.codec) {
            case PSX:
            case PSX_bf:        
            case HEVAG:         interleave = 0x10; break;
            case NGC_DSP:       interleave = 0x08; break;
            case PCM16LE:
            case PCM16BE:       interleave = 0x02; break;
            case PCM8:
            case PCM8_U:
            case PCM8_SB:       interleave = 0x01; break;
            case PCM_FLOAT_LE:  interleave = 0x04; break;
            default:
                 break;
        }
        txth.interleave = interleave;
    }


    /* type to coding conversion */
    switch (txth.codec) {
        case PSX:           coding = coding_PSX; break;
        case PSX_bf:        coding = coding_PSX_badflags; break;
        case HEVAG:         coding = coding_HEVAG; break;
        case XBOX:          coding = coding_XBOX_IMA; break;
        case NGC_DTK:       coding = coding_NGC_DTK; break;
        case PCM16LE:       coding = coding_PCM16LE; break;
        case PCM16BE:       coding = coding_PCM16BE; break;
        case PCM8:          coding = coding_PCM8; break;
        case PCM8_U:        coding = coding_PCM8_U; break;
        case PCM8_U_int:    coding = coding_PCM8_U_int; break;
        case PCM8_SB:       coding = coding_PCM8_SB; break;
        case PCM_FLOAT_LE:  coding = coding_PCMFLOAT; break;
        case SDX2:          coding = coding_SDX2; break;
        case DVI_IMA:       coding = coding_DVI_IMA; break;
        case IMA_HV:        coding = coding_HV_IMA; break;
#ifdef VGM_USE_MPEG
        case MPEG:          coding = coding_MPEG_layer3; break; /* we later find out exactly which */
#endif
        case IMA:           coding = coding_IMA; break;
        case YMZ:
        case AICA:          coding = coding_AICA; break;
        case MSADPCM:       coding = coding_MSADPCM; break;
        case NGC_DSP:       coding = coding_NGC_DSP; break;
        case MS_IMA:        coding = coding_MS_IMA; break;
        case APPLE_IMA4:    coding = coding_APPLE_IMA4; break;
#ifdef VGM_USE_FFMPEG
        case ATRAC3:
        case ATRAC3PLUS:
        case XMA1:
        case XMA2:
        case AC3:
        case AAC:
        case FFMPEG:        coding = coding_FFmpeg; break;
#endif
        case PCFX:          coding = coding_PCFX; break;
        case PCM4:          coding = coding_PCM4; break;
        case PCM4_U:        coding = coding_PCM4_U; break;
        case OKI16:         coding = coding_OKI16; break;
        case OKI4S:         coding = coding_OKI4S; break;
        case TGC:           coding = coding_TGC; break;
        case ASF:           coding = coding_ASF; break;
        case EAXA:          coding = coding_EA_XA; break;
        case XA:            coding = coding_XA; break;
        case XA_EA:         coding = coding_XA_EA; break;
        case CP_YM:         coding = coding_CP_YM; break;
        default:
            goto fail;
    }


    /* try to autodetect PS-ADPCM loop data */
    if (txth.loop_flag_auto && coding == coding_PSX) {
        txth.loop_flag = ps_find_loop_offsets(txth.sf_body, txth.start_offset, txth.data_size, txth.channels, txth.interleave,
                (int32_t*)&txth.loop_start_sample, (int32_t*)&txth.loop_end_sample);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(txth.channels,txth.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = txth.sample_rate;
    vgmstream->num_samples = txth.num_samples;
    vgmstream->loop_start_sample = txth.loop_start_sample;
    vgmstream->loop_end_sample = txth.loop_end_sample;
    vgmstream->num_streams = txth.subsong_count;
    vgmstream->stream_size = txth.data_size;
    if (txth.name_offset_set) {
        size_t name_size = txth.name_size ? txth.name_size + 1 : STREAM_NAME_SIZE;
        read_string(vgmstream->stream_name,name_size, txth.name_offset,txth.sf_head);
    }

    /* codec specific (taken from GENH with minimal changes) */
    switch (coding) {
        case coding_PCM8_U_int:
            vgmstream->layout_type = layout_none;
            break;
        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM8:
        case coding_PCM8_U:
        case coding_PCM8_SB:
        case coding_PCMFLOAT:
        case coding_PCM4:
        case coding_PCM4_U:
        case coding_SDX2:
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
        case coding_DVI_IMA:
        case coding_IMA:
        case coding_HV_IMA:
        case coding_AICA:
        case coding_APPLE_IMA4:
        case coding_TGC:
            vgmstream->interleave_block_size = txth.interleave;
            vgmstream->interleave_last_block_size = txth.interleave_last;
            if (vgmstream->channels > 1)
            {
                if (coding == coding_SDX2) {
                    coding = coding_SDX2_int;
                }

                if (vgmstream->interleave_block_size==0xffffffff || vgmstream->interleave_block_size == 0) {
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
                if (!txth.interleave && (
                        coding == coding_PSX ||
                        coding == coding_PSX_badflags ||
                        coding == coding_HEVAG ||
                        coding == coding_IMA_int ||
                        coding == coding_DVI_IMA_int ||
                        coding == coding_SDX2_int ||
                        coding == coding_AICA_int) ) {
                    goto fail;
                }
            } else {
                vgmstream->layout_type = layout_none;
            }

            /* to avoid problems with dual stereo files (_L+_R) for codecs with stereo modes */
            if (coding == coding_AICA && txth.channels == 1)
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
                vgmstream->codec_config = txth.codec_mode;
            }

            if (txth.codec == YMZ) {
                vgmstream->codec_config = 1; /* CONFIG_HIGH_NIBBLE */
            }

            //TODO recheck and use only for needed cases
            vgmstream->allow_dual_stereo = 1; /* known to be used in: PSX, AICA, YMZ */
            break;

        case coding_PCFX:
            vgmstream->interleave_block_size = txth.interleave;
            vgmstream->interleave_last_block_size = txth.interleave_last;
            vgmstream->layout_type = layout_interleave;
            if (txth.codec_mode >= 0 && txth.codec_mode <= 3)
                vgmstream->codec_config = txth.codec_mode;
            break;

        case coding_OKI16:
        case coding_OKI4S:
        case coding_XA:
        case coding_XA_EA:
        case coding_CP_YM:
            vgmstream->layout_type = layout_none;
            break;

        case coding_ASF:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x11;
            break;

        case coding_EA_XA: /* from 'raw' modes in sx.exe [Harry Potter and the Chamber of Secrets (PC)] */
            if (txth.codec_mode == 1) { /* mono interleave */
                coding = coding_EA_XA_int;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = txth.interleave;
                vgmstream->interleave_last_block_size = txth.interleave_last;
            } else { /* mono/stereo */
                if (vgmstream->channels > 2)
                    goto fail; /* only 1ch and 2ch are known */

                vgmstream->layout_type = layout_none;
            }
            break;

        case coding_MS_IMA:
            if (!txth.interleave) goto fail; /* creates garbage */

            vgmstream->interleave_block_size = txth.interleave;
            vgmstream->layout_type = layout_none;

            vgmstream->allow_dual_stereo = 1; //???
            break;

        case coding_MSADPCM:
            if (vgmstream->channels > 2) goto fail;
            if (!txth.interleave) goto fail;

            vgmstream->frame_size = txth.interleave;
            vgmstream->layout_type = layout_none;
            break;

        case coding_XBOX_IMA:
            if (txth.codec_mode == 1) { /* mono interleave */
                coding = coding_XBOX_IMA_int;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = txth.interleave;
                vgmstream->interleave_last_block_size = txth.interleave_last;
            }
            else { /* 1ch mono, or stereo interleave */
                vgmstream->layout_type = txth.interleave ? layout_interleave : layout_none;
                vgmstream->interleave_block_size = txth.interleave;
                vgmstream->interleave_last_block_size = txth.interleave_last;
                if (vgmstream->channels > 2 && vgmstream->channels % 2 != 0)
                    goto fail; /* only 2ch+..+2ch layout is known */
            }
            break;

        case coding_NGC_DTK:
            if (vgmstream->channels != 2) goto fail;
            vgmstream->layout_type = layout_none;
            break;

        case coding_NGC_DSP:
            if (txth.channels > 1 && txth.codec_mode == 0) {
                if (!txth.interleave) goto fail;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_last_block_size = txth.interleave_last;
                vgmstream->interleave_block_size = txth.interleave;
            } else if (txth.channels > 1 && txth.codec_mode == 1) {
                if (!txth.interleave) goto fail;
                coding = coding_NGC_DSP_subint;
                vgmstream->layout_type = layout_none;
                vgmstream->interleave_block_size = txth.interleave;
            } else if (txth.channels == 1 || txth.codec_mode == 2) {
                vgmstream->layout_type = layout_none;
            } else {
                goto fail;
            }

            /* get coefs */
            {
                read_s16_t read_s16 = txth.coef_big_endian ? read_s16be : read_s16le;
                get_s16_t get_s16 =txth.coef_big_endian ? get_s16be : get_s16le;

                for (i = 0; i < vgmstream->channels; i++) {
                    if (txth.coef_mode == 0) { /* normal coefs */
                        for (j = 0; j < 16; j++) {
                            int16_t coef;
                            if (txth.coef_table_set)
                                coef =  get_s16(txth.coef_table  + i*txth.coef_spacing  + j*2);
                            else
                                coef = read_s16(txth.coef_offset + i*txth.coef_spacing  + j*2, txth.sf_head);
                            vgmstream->ch[i].adpcm_coef[j] = coef;
                        }
                    }
                    else { /* split coefs (first all 8 positive, then all 8 negative [P.N.03 (GC), Viewtiful Joe (GC)] */
                        for (j = 0; j < 8; j++) {
                            vgmstream->ch[i].adpcm_coef[j*2+0] = read_s16(txth.coef_offset + i*txth.coef_spacing + j*2 + 0x00, txth.sf_head);
                            vgmstream->ch[i].adpcm_coef[j*2+1] = read_s16(txth.coef_offset + i*txth.coef_spacing + j*2 + 0x10, txth.sf_head);
                        }
                    }
                }
            }

            /* get hist */
            if (txth.hist_set) {
                read_s16_t read_s16 = txth.coef_big_endian ? read_s16be : read_s16le;

                for (i = 0; i < vgmstream->channels; i++) {
                    off_t offset = txth.hist_offset + i*txth.hist_spacing;
                    vgmstream->ch[i].adpcm_history1_16 = read_s16(offset + 0x00, txth.sf_head);
                    vgmstream->ch[i].adpcm_history2_16 = read_s16(offset + 0x02, txth.sf_head);
                }
            }

            vgmstream->allow_dual_stereo = 1;
            break;

#ifdef VGM_USE_MPEG
        case coding_MPEG_layer3:
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_mpeg(txth.sf_body, txth.start_offset, &coding, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *ffmpeg_data = NULL;

            if (txth.codec == FFMPEG || txth.codec == AC3) {
                /* default FFmpeg */
                ffmpeg_data = init_ffmpeg_offset(txth.sf_body, txth.start_offset, txth.data_size);
                if (!ffmpeg_data) goto fail;

                if (vgmstream->num_samples == 0)
                    vgmstream->num_samples = ffmpeg_get_samples(ffmpeg_data); /* sometimes works */
            }
            else if (txth.codec == AAC) {
                ffmpeg_data = init_ffmpeg_aac(txth.sf_body, txth.start_offset, txth.data_size, 0);
                if (!ffmpeg_data) goto fail;
            }
            else {
                /* fake header FFmpeg */
                uint8_t buf[0x100];
                int32_t bytes;

                if (txth.codec == ATRAC3) {
                    int block_align, encoder_delay;

                    block_align = txth.interleave;
                    encoder_delay = txth.skip_samples;

                    ffmpeg_data = init_ffmpeg_atrac3_raw(txth.sf_body, txth.start_offset,txth.data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
                    if (!ffmpeg_data) goto fail;
                }
                else if (txth.codec == ATRAC3PLUS) {
                    int block_size = txth.interleave;

                    bytes = ffmpeg_make_riff_atrac3plus(buf, sizeof(buf), vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, block_size, txth.skip_samples);
                    ffmpeg_data = init_ffmpeg_header_offset(txth.sf_body, buf,bytes, txth.start_offset,txth.data_size);
                    if ( !ffmpeg_data ) goto fail;
                }
                else if (txth.codec == XMA1) {
                    int xma_stream_mode = txth.codec_mode == 1 ? 1 : 0;

                    bytes = ffmpeg_make_riff_xma1(buf, sizeof(buf), vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, xma_stream_mode);
                    ffmpeg_data = init_ffmpeg_header_offset(txth.sf_body, buf,bytes, txth.start_offset,txth.data_size);
                    if ( !ffmpeg_data ) goto fail;
                }
                else if (txth.codec == XMA2) {
                    int block_count, block_size;

                    block_size = txth.interleave ? txth.interleave : 2048;
                    block_count = txth.data_size / block_size;

                    bytes = ffmpeg_make_riff_xma2(buf, sizeof(buf), vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
                    ffmpeg_data = init_ffmpeg_header_offset(txth.sf_body, buf,bytes, txth.start_offset,txth.data_size);
                    if ( !ffmpeg_data ) goto fail;
                }
                else {
                    goto fail;
                }
            }

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->layout_type = layout_none;

            if (txth.codec == XMA1 || txth.codec == XMA2) {
                xma_fix_raw_samples(vgmstream, txth.sf_body, txth.start_offset,txth.data_size, 0, 0,0);
            } else if (txth.skip_samples_set && txth.codec != ATRAC3) { /* force encoder delay */
                ffmpeg_set_skip_samples(ffmpeg_data, txth.skip_samples);
            }

            break;
        }
#endif
        default:
            break;
    }

#ifdef VGM_USE_FFMPEG
    if ((txth.sample_type==1 || txth.num_samples_data_size) && (txth.codec == XMA1 || txth.codec == XMA2)) {
        /* manually find sample offsets */
        ms_sample_data msd = {0};

        msd.xma_version = 1;
        msd.channels = txth.channels;
        msd.data_offset = txth.start_offset;
        msd.data_size = txth.data_size;
        if (txth.sample_type==1) {
            msd.loop_flag = txth.loop_flag;
            msd.loop_start_b = txth.loop_start_sample;
            msd.loop_end_b   = txth.loop_end_sample;
            msd.loop_start_subframe = txth.loop_adjust & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
            msd.loop_end_subframe   = txth.loop_adjust >> 4;  /* upper 4b: subframe where the loop ends, 0..3 */
        }

        xma_get_samples(&msd, txth.sf_body);

        vgmstream->num_samples = msd.num_samples;
        if (txth.sample_type==1) {
            vgmstream->loop_start_sample = msd.loop_start_sample;
            vgmstream->loop_end_sample = msd.loop_end_sample;
        }
    }
#endif

    if (vgmstream->interleave_block_size) {
        if (txth.interleave_first_skip && !txth.interleave_first)
            txth.interleave_first = txth.interleave;
        if (txth.interleave_first > txth.interleave_first_skip)
            txth.interleave_first -= txth.interleave_first_skip;
        vgmstream->interleave_first_block_size = txth.interleave_first;
        vgmstream->interleave_first_skip = txth.interleave_first_skip;
        txth.start_offset += txth.interleave_first_skip;
    }


    vgmstream->coding_type = coding;
    vgmstream->meta_type = meta_TXTH;


    if (!vgmstream_open_stream(vgmstream, txth.sf_body, txth.start_offset))
        goto fail;

    clean_txth(&txth);
    return vgmstream;

fail:
    clean_txth(&txth);
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* init_subfile(txth_header* txth) {
    VGMSTREAM* vgmstream = NULL;
    char extension[PATH_LIMIT];
    STREAMFILE* sf_sub = NULL;


    if (txth->subfile_size == 0) {
        if (txth->data_size_set)
            txth->subfile_size = txth->data_size;
        else
            txth->subfile_size = txth->data_size - txth->subfile_offset;
        if (txth->subfile_size + txth->subfile_offset > get_streamfile_size(txth->sf_body))
            txth->subfile_size = get_streamfile_size(txth->sf_body) - txth->subfile_offset;
    }

    if (txth->subfile_extension[0] == '\0')
        get_streamfile_ext(txth->sf,txth->subfile_extension,sizeof(txth->subfile_extension));

    /* must detect a potential infinite loop:
     * - init_vgmstream enters TXTH and reads .txth
     * - TXTH subfile calls init, nothing is detected
     * - init_vgmstream enters TXTH and reads .txth
     * - etc
     * to avoid it we set a particular fake extension and detect it when reading .txth
     */
    strcpy(extension, ".subfile_txth.");
    strcat(extension, txth->subfile_extension);

    sf_sub = setup_subfile_streamfile(txth->sf_body, txth->subfile_offset, txth->subfile_size, extension);
    if (!sf_sub) goto fail;

    sf_sub->stream_index = txth->sf->stream_index;

    vgmstream = init_vgmstream_from_STREAMFILE(sf_sub);
    if (!vgmstream) {
        /* In case of subfiles with subsongs pass subsong N by default (ex. subfile is a .fsb with N subsongs).
         * But if the subfile is a single-subsong subfile (ex. subfile is a .fsb with 1 subsong) try again
         * without passing index (as it would fail first trying to open subsong N). */
        if (sf_sub->stream_index > 1) {
            sf_sub->stream_index = 0;
            vgmstream = init_vgmstream_from_STREAMFILE(sf_sub);
            if (!vgmstream) goto fail;
        }
        else {
            goto fail;
        }
    }

    /* apply some fields */
    if (txth->sample_rate)
        vgmstream->sample_rate = txth->sample_rate;
    if (txth->num_samples)
        vgmstream->num_samples = txth->num_samples;

    /* load some fields for possible calcs */
    if (!txth->channels)
        txth->channels = vgmstream->channels;
    if (!txth->sample_rate)
        txth->sample_rate = vgmstream->sample_rate;
    if (!txth->interleave)
        txth->interleave = vgmstream->interleave_block_size;
    if (!txth->interleave_last)
        txth->interleave_last = vgmstream->interleave_last_block_size;
    if (!txth->interleave_first)
        txth->interleave_first = vgmstream->interleave_first_block_size;
    if (!txth->interleave_first_skip)
        txth->interleave_first_skip = vgmstream->interleave_first_skip;
    //if (!txth->loop_flag) //?
    //    txth->loop_flag = vgmstream->loop_flag;
    /* sometimes headers set loop start but getting loop_end before subfile init is hard */
    if (!txth->loop_end_sample && txth->loop_flag)
        txth->loop_end_sample = vgmstream->num_samples;

    /* other init */
    if (txth->loop_flag) {
        vgmstream_force_loop(vgmstream, txth->loop_flag, txth->loop_start_sample, txth->loop_end_sample);
    }
    else if (txth->loop_flag_set && vgmstream->loop_flag) {
        vgmstream_force_loop(vgmstream, 0, 0, 0);
    }

    /* assumes won't point to subfiles with subsongs */
    if (/*txth->chunk_count &&*/ txth->subsong_count) {
        vgmstream->num_streams = txth->subsong_count;
    }
    //todo: other combos with subsongs + subfile?


    close_streamfile(sf_sub);
    return vgmstream;

fail:
    close_streamfile(sf_sub);
    close_vgmstream(vgmstream);
    return NULL;
}


static STREAMFILE* open_txth(STREAMFILE* sf) {
    char filename[PATH_LIMIT];
    const char* base_ext;
    const char* txth_ext;
    STREAMFILE* sf_text;


    get_streamfile_name(sf, filename, sizeof(filename));
    if (strstr(filename, ".subfile_txth") != NULL)
        return NULL; /* detect special case of subfile-within-subfile */

    base_ext = filename_extension(filename);
    concatn(sizeof(filename), filename, ".txth");
    txth_ext = filename_extension(filename);

    /* try "(path/)(name.ext).txth" */
    {
        /* full filename, already prepared */

        sf_text = open_streamfile(sf, filename);
        if (sf_text) return sf_text;
    }

    /* try "(path/)(.ext).txth" */
    if (base_ext) {
        base_ext--; //get_streamfile_path(sf, filename, sizeof(filename));

        sf_text = open_streamfile_by_filename(sf, base_ext);
        if (sf_text) return sf_text;
    }

    /* try "(path/).txth" */
    if (txth_ext) {
        txth_ext--; /* points to "txth" due to the concat */

        sf_text = open_streamfile_by_filename(sf, txth_ext);
        if (sf_text) return sf_text;
    }

    /* not found */
    return NULL;
}

static void clean_txth(txth_header* txth) {
    /* close stuff manually opened during parse */
    if (txth->sf_text_opened) close_streamfile(txth->sf_text);
    if (txth->sf_head_opened) close_streamfile(txth->sf_head);
    if (txth->sf_body_opened) close_streamfile(txth->sf_body);
}

/* ****************************************************************** */

static void set_body_chunk(txth_header* txth) {
    STREAMFILE* temp_sf = NULL;

    /* sets body "chunk" if all needed values are set
     * (done inline for padding/get_samples/etc calculators to work) */
    //todo maybe should only be done once, or have some count to retrigger to simplify?
    if (!txth->chunk_start_set || !txth->chunk_size_set || !txth->chunk_count_set)
        return;
    if ((txth->chunk_size == 0 && ! txth->chunk_size_offset) ||
         txth->chunk_start > txth->data_size ||
         txth->chunk_count == 0)
        return;
    if (!txth->sf_body)
        return;

    /* treat chunks as subsongs */
    if (txth->subsong_count > 1 && txth->subsong_count == txth->chunk_count)
        txth->chunk_number = txth->target_subsong;
    if (txth->chunk_number == 0)
        txth->chunk_number = 1;
    if (txth->chunk_number > txth->chunk_count)
        return;

    {
        txth_io_config_data cfg = {0};

        cfg.chunk_number = txth->chunk_number - 1; /* 1-index to 0-index */
        cfg.chunk_header_size = txth->chunk_header_size;
        cfg.chunk_data_size = txth->chunk_data_size;

        cfg.chunk_value = txth->chunk_value;
        cfg.chunk_size_offset = txth->chunk_size_offset;
        cfg.chunk_be = txth->chunk_be;

        cfg.chunk_start = txth->chunk_start;
        cfg.chunk_size = txth->chunk_size;
        cfg.chunk_count = txth->chunk_count;

        temp_sf = setup_txth_streamfile(txth->sf_body, cfg, txth->sf_body_opened);
        if (!temp_sf) return;
    }

    /* closing is handled by temp_sf */
    //if (txth->sf_body_opened) {
    //    close_streamfile(txth->sf_body);
    //    txth->sf_body = NULL;
    //    txth->sf_body_opened = 0;
    //}

    txth->sf_body = temp_sf;
    txth->sf_body_opened = 1;

    /* cancel values once set, to avoid weirdness and possibly allow chunks-in-chunks? */
    txth->chunk_start_set = 0;
    txth->chunk_size_set = 0;
    txth->chunk_count_set = 0;

    /* re-apply */
    if (!txth->data_size_set) {
        txth->data_size = get_streamfile_size(txth->sf_body);
    }
}

static int parse_keyval(STREAMFILE* sf, txth_header* txth, const char* key, char* val);
static int parse_num(STREAMFILE* sf, txth_header* txth, const char* val, uint32_t* out_value);
static int parse_string(STREAMFILE* sf, txth_header* txth, const char* val, char* str);
static int parse_coef_table(STREAMFILE* sf, txth_header* txth, const char* val, uint8_t* out_value, size_t out_size);
static int parse_name_table(txth_header* txth, char* val);
static int parse_multi_txth(txth_header* txth, char* val);
static int is_string(const char* val, const char* cmp);
static int get_bytes_to_samples(txth_header* txth, uint32_t bytes);
static int get_padding_size(txth_header* txth, int discard_empty);

/* Simple text parser of "key = value" lines.
 * The code is meh and error handling not exactly the best. */
static int parse_txth(txth_header* txth) {
    uint32_t txt_offset;

    /* setup txth defaults */
    if (txth->sf_body)
        txth->data_size = get_streamfile_size(txth->sf_body);
    txth->target_subsong = txth->sf->stream_index;
    if (txth->target_subsong == 0) txth->target_subsong = 1;


    txt_offset = read_bom(txth->sf_text);

    /* read lines */
    {
        text_reader_t tr;
        uint8_t buf[TXT_LINE_MAX + 1];
        char key[TXT_LINE_KEY_MAX];
        char val[TXT_LINE_VAL_MAX];
        int ok, line_len;
        char* line;

        if (!text_reader_init(&tr, buf, sizeof(buf), txth->sf_text, txt_offset, 0))
            goto fail;

        do {
            line_len = text_reader_get_line(&tr, &line);
            if (line_len < 0) goto fail; /* too big for buf (maybe not text)) */

            if (line == NULL) /* EOF */
                break;

            if (line_len == 0) /* empty */
                continue;

            /* get key/val (ignores lead spaces, stops at space/comment/separator) */
            ok = sscanf(line, " %[^ \t#=] = %[^\t#\r\n] ", key,val);
            if (ok != 2) /* ignore line if no key=val (comment or garbage) */
                continue;

            if (!parse_keyval(txth->sf, txth, key, val)) /* read key/val */
                goto fail;

        } while (line_len >= 0);
    }

    if (!txth->loop_flag_set)
        txth->loop_flag = txth->loop_end_sample && txth->loop_end_sample != 0xFFFFFFFF;

    if (!txth->sf_body)
        goto fail;

    if (txth->data_size > get_streamfile_size(txth->sf_body) - txth->start_offset || txth->data_size <= 0)
        txth->data_size = get_streamfile_size(txth->sf_body) - txth->start_offset;

    return 1;
fail:
    return 0;
}

static txth_codec_t parse_codec(txth_header* txth, const char* val) {
    if      (is_string(val,"PSX"))          return PSX;
    else if (is_string(val,"XBOX"))         return XBOX;
    else if (is_string(val,"NGC_DTK"))      return NGC_DTK;
    else if (is_string(val,"DTK"))          return NGC_DTK;
    else if (is_string(val,"PCM16BE"))      return PCM16BE;
    else if (is_string(val,"PCM16LE"))      return PCM16LE;
    else if (is_string(val,"PCM8"))         return PCM8;
    else if (is_string(val,"PCM8_U"))       return PCM8_U;
    else if (is_string(val,"PCM8_U_int"))   return PCM8_U_int;
    else if (is_string(val,"PCM8_SB"))      return PCM8_SB;
    else if (is_string(val,"SDX2"))         return SDX2;
    else if (is_string(val,"DVI_IMA"))      return DVI_IMA;
    else if (is_string(val,"MPEG"))         return MPEG;
    else if (is_string(val,"IMA"))          return IMA;
    else if (is_string(val,"AICA"))         return AICA;
    else if (is_string(val,"YMZ"))          return YMZ;
    else if (is_string(val,"MSADPCM"))      return MSADPCM;
    else if (is_string(val,"NGC_DSP"))      return NGC_DSP;
    else if (is_string(val,"DSP"))          return NGC_DSP;
    else if (is_string(val,"PSX_bf"))       return PSX_bf;
    else if (is_string(val,"MS_IMA"))       return MS_IMA;
    else if (is_string(val,"APPLE_IMA4"))   return APPLE_IMA4;
    else if (is_string(val,"ATRAC3"))       return ATRAC3;
    else if (is_string(val,"ATRAC3PLUS"))   return ATRAC3PLUS;
    else if (is_string(val,"XMA1"))         return XMA1;
    else if (is_string(val,"XMA2"))         return XMA2;
    else if (is_string(val,"FFMPEG"))       return FFMPEG;
    else if (is_string(val,"AC3"))          return AC3;
    else if (is_string(val,"PCFX"))         return PCFX;
    else if (is_string(val,"PCM4"))         return PCM4;
    else if (is_string(val,"PCM4_U"))       return PCM4_U;
    else if (is_string(val,"OKI16"))        return OKI16;
    else if (is_string(val,"OKI4S"))        return OKI4S;
    else if (is_string(val,"AAC"))          return AAC;
    else if (is_string(val,"TGC"))          return TGC;
    else if (is_string(val,"GCOM_ADPCM"))   return TGC;
    else if (is_string(val,"ASF"))          return ASF;
    else if (is_string(val,"EAXA"))         return EAXA;
    else if (is_string(val,"XA"))           return XA;
    else if (is_string(val,"XA_EA"))        return XA_EA;
    else if (is_string(val,"CP_YM"))        return CP_YM;
    else if (is_string(val,"PCM_FLOAT_LE")) return PCM_FLOAT_LE;
    else if (is_string(val,"IMA_HV"))       return IMA_HV;
    else if (is_string(val,"HEVAG"))        return HEVAG;
    /* special handling */
    else if (is_string(val,"name_value"))   return txth->name_values[0];
    else if (is_string(val,"name_value1"))  return txth->name_values[0];
    else if (is_string(val,"name_value2"))  return txth->name_values[1];
    else if (is_string(val,"name_value3"))  return txth->name_values[2];
    //todo rest (handle in function)

    return UNKNOWN;
}

static int parse_endianness(txth_header* txth, const char* val, uint32_t* p_value, uint32_t* mode) {
    if (is_string(val, "BE")) {
        *p_value = 1;
        if (mode) *mode = 0;
    }
    else if (is_string(val, "LE")) {
        *p_value = 0;
        if (mode) *mode = 0;
    }
    else if (is_string(val, "BE_split")) {
        *p_value = 1;
        if (mode) *mode = 1;
    }
    else if (is_string(val, "LE_split")) {
        *p_value = 0;
        if (mode) *mode = 1;
    }
    else {
        if (!parse_num(txth->sf_head,txth,val, p_value))
             goto fail;
    }
    return 1;
fail:
    return 0;
}

static int parse_keyval(STREAMFILE* sf_, txth_header* txth, const char* key, char* val) {

    if (txth->debug)
        vgm_logi("TXTH: reading key=%s, val=%s\n", key, val);


    /* CODEC */
    if (is_string(key,"codec")) {
        txth->codec = parse_codec(txth, val);
        if (txth->codec == UNKNOWN)
            goto fail;
    }
    else if (is_string(key,"codec_mode")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->codec_mode)) goto fail;
    }

    /* VALUE MODIFIERS */
    else if (is_string(key,"value_mul") || is_string(key,"value_*")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->value_mul)) goto fail;
    }
    else if (is_string(key,"value_div") || is_string(key,"value_/")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->value_div)) goto fail;
    }
    else if (is_string(key,"value_add") || is_string(key,"value_+")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->value_add)) goto fail;
    }
    else if (is_string(key,"value_sub") || is_string(key,"value_-")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->value_sub)) goto fail;
    }

    /* ID VALUES */
    else if (is_string(key,"id_value")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->id_value)) goto fail;
    }
    else if (is_string(key,"id_check") || is_string(key,"id_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->id_check)) goto fail;
        if (txth->id_value != txth->id_check) /* evaluate current ID */
            goto fail;
    }

    /* INTERLEAVE / FRAME SIZE */
    else if (is_string(key,"interleave")) {
        if (is_string(val,"half_size")) {
            if (txth->channels == 0) goto fail;
            txth->interleave = txth->data_size / txth->channels;
        }
        else {
            if (!parse_num(txth->sf_head,txth,val, &txth->interleave)) goto fail;
        }
    }
    else if (is_string(key,"interleave_last")) {
        if (is_string(val,"auto")) {
            if (txth->channels > 0 && txth->interleave > 0)
                txth->interleave_last = (txth->data_size % (txth->interleave * txth->channels)) / txth->channels;
        }
        else {
            if (!parse_num(txth->sf_head,txth,val, &txth->interleave_last)) goto fail;
        }
    }
    else if (is_string(key,"interleave_first")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->interleave_first)) goto fail;
    }
    else if (is_string(key,"interleave_first_skip")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->interleave_first_skip)) goto fail;

        /* apply */
        if (!txth->data_size_set) {
            int skip = txth->interleave_first_skip * txth->channels;
            if (txth->data_size && txth->data_size > skip)
                txth->data_size -= skip;
        }
    }

    /* BASE CONFIG */
    else if (is_string(key,"channels")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->channels)) goto fail;
    }
    else if (is_string(key,"sample_rate")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->sample_rate)) goto fail;
    }

    /* DATA CONFIG */
    else if (is_string(key,"start_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->start_offset)) goto fail;

        /* apply */
        if (!txth->data_size_set) {

            //TODO: this doesn't work when using name_table + subsongs, since values are pre-read
            /* with subsongs we want to clamp data_size from this subsong start to next subsong start */
            txth->next_offset = txth->data_size;
            if (txth->subsong_count > 1 && txth->target_subsong < txth->subsong_count) {
                /* temp move to next start_offset and move back*/
                txth->target_subsong++;
                parse_num(txth->sf_head,txth,val, &txth->next_offset);
                txth->target_subsong--;
                if (txth->next_offset < txth->start_offset)
                    txth->next_offset = 0;
            }

            if (txth->data_size && txth->data_size > txth->next_offset && txth->next_offset)
                txth->data_size = txth->next_offset;
            if (txth->data_size && txth->data_size > txth->start_offset)
                txth->data_size -= txth->start_offset;
        }
    }
    else if (is_string(key,"padding_size")) {
        if (is_string(val,"auto")) {
            txth->padding_size = get_padding_size(txth, 0);
        }
        else if (is_string(val,"auto-empty")) {
            txth->padding_size = get_padding_size(txth, 1);
        }
        else {
            if (!parse_num(txth->sf_head,txth,val, &txth->padding_size)) goto fail;
        }

        /* apply */
        if (!txth->data_size_set) {
            if (txth->data_size && txth->data_size > txth->padding_size)
                txth->data_size -= txth->padding_size;
        }
    }
    else if (is_string(key,"data_size")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->data_size)) goto fail;
        txth->data_size_set = 1;
    }

    /* SAMPLES */
    else if (is_string(key,"sample_type")) {
        if (is_string(val,"samples")) txth->sample_type = 0;
        else if (is_string(val,"bytes")) txth->sample_type = 1;
        else if (is_string(val,"blocks")) txth->sample_type = 2;
        else goto fail;
    }
    else if (is_string(key,"num_samples")) {
        if (is_string(val,"data_size")) {
            txth->num_samples = get_bytes_to_samples(txth, txth->data_size);
            txth->num_samples_data_size = 1;
        }
        else {
            if (!parse_num(txth->sf_head,txth,val, &txth->num_samples)) goto fail;
            if (txth->sample_type==1)
                txth->num_samples = get_bytes_to_samples(txth, txth->num_samples);
            if (txth->sample_type==2)
                txth->num_samples = get_bytes_to_samples(txth, txth->num_samples * (txth->interleave*txth->channels));
        }
    }
    else if (is_string(key,"loop_start_sample") || is_string(key,"loop_start")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->loop_start_sample)) goto fail;
        if (txth->sample_type==1)
            txth->loop_start_sample = get_bytes_to_samples(txth, txth->loop_start_sample);
        if (txth->sample_type==2)
            txth->loop_start_sample = get_bytes_to_samples(txth, txth->loop_start_sample * (txth->interleave*txth->channels));
        if (txth->loop_adjust)
            txth->loop_start_sample += txth->loop_adjust;
    }
    else if (is_string(key,"loop_end_sample") || is_string(key,"loop_end")) {
        if (is_string(val,"data_size")) {
            txth->loop_end_sample = get_bytes_to_samples(txth, txth->data_size);
        }
        else {
            if (!parse_num(txth->sf_head,txth,val, &txth->loop_end_sample)) goto fail;
            if (txth->sample_type==1)
                txth->loop_end_sample = get_bytes_to_samples(txth, txth->loop_end_sample);
            if (txth->sample_type==2)
                txth->loop_end_sample = get_bytes_to_samples(txth, txth->loop_end_sample * (txth->interleave*txth->channels));
        }
        if (txth->loop_adjust)
            txth->loop_end_sample += txth->loop_adjust;
    }
    else if (is_string(key,"skip_samples")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->skip_samples)) goto fail;
        txth->skip_samples_set = 1;
        if (txth->sample_type==1)
            txth->skip_samples = get_bytes_to_samples(txth, txth->skip_samples);
        if (txth->sample_type==2)
            txth->skip_samples = get_bytes_to_samples(txth, txth->skip_samples * (txth->interleave*txth->channels));
    }
    else if (is_string(key,"loop_adjust")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->loop_adjust)) goto fail;
        if (txth->sample_type==1)
            txth->loop_adjust = get_bytes_to_samples(txth, txth->loop_adjust);
        if (txth->sample_type==2)
            txth->loop_adjust = get_bytes_to_samples(txth, txth->loop_adjust * (txth->interleave*txth->channels));
    }
    else if (is_string(key,"loop_flag")) {
        if (is_string(val,"auto"))  {
            txth->loop_flag_auto = 1;
        }
        else {
            if (!parse_num(txth->sf_head,txth,val, &txth->loop_flag)) goto fail;
            txth->loop_flag_set = 1;

            if (txth->loop_behavior == DEFAULT) {
                if ((txth->loop_flag == 0xFFFF || txth->loop_flag == 0xFFFFFFFF) )
                    txth->loop_flag = 0;
            }
            else if (txth->loop_behavior == NEGATIVE) {
                if (txth->loop_flag == 0xFF || txth->loop_flag == 0xFFFF || txth->loop_flag == 0xFFFFFFFF)
                    txth->loop_flag = 1;
            }
            else if (txth->loop_behavior == POSITIVE) {
                if (txth->loop_flag == 0xFF || txth->loop_flag == 0xFFFF || txth->loop_flag == 0xFFFFFFFF)
                    txth->loop_flag = 0;
                else if (txth->loop_flag == 0)
                    txth->loop_flag = 1;
            }
            else if (txth->loop_behavior == INVERTED) {
                txth->loop_flag = (txth->loop_flag == 0);
            }
        }
    }
    else if (is_string(key,"loop_behavior")) {
        if (is_string(val, "default"))
            txth->loop_behavior = DEFAULT;
        else if (is_string(val, "negative"))
            txth->loop_behavior = NEGATIVE;
        else if (is_string(val, "positive"))
            txth->loop_behavior = POSITIVE;
        else if (is_string(val, "inverted"))
            txth->loop_behavior = INVERTED;
        else
            goto fail;
    }

    else if (is_string(key,"offset_absolute")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->is_offset_absolute)) goto fail;
    }

    /* COEFS */
    else if (is_string(key,"coef_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->coef_offset)) goto fail;
        /* special adjustments */
        txth->coef_offset += txth->base_offset;
        if (txth->subsong_spacing && !txth->is_offset_absolute)
            txth->coef_offset += txth->subsong_spacing * (txth->target_subsong - 1);
    }
    else if (is_string(key,"coef_spacing")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->coef_spacing)) goto fail;
    }
    else if (is_string(key,"coef_endianness")) {
        if (!parse_endianness(txth, val, &txth->coef_big_endian, &txth->coef_mode)) goto fail;
    }
    else if (is_string(key,"coef_table")) {
        if (!parse_coef_table(txth->sf_head,txth,val, txth->coef_table, sizeof(txth->coef_table))) goto fail;
        txth->coef_table_set = 1;
    }

    /* HIST */
    else if (is_string(key,"hist_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->hist_offset)) goto fail;
        txth->hist_set = 1;
        /* special adjustment */
        txth->hist_offset += txth->hist_offset;
        if (txth->subsong_spacing && !txth->is_offset_absolute)
            txth->hist_offset += txth->subsong_spacing * (txth->target_subsong - 1);
    }
    else if (is_string(key,"hist_spacing")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->hist_spacing)) goto fail;
    }
    else if (is_string(key,"hist_endianness")) {
        if (!parse_endianness(txth, val, &txth->hist_big_endian, NULL)) goto fail;
    }

    /* SUBSONGS */
    else if (is_string(key,"subsong_count")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->subsong_count)) goto fail;
    }
    else if (is_string(key,"subsong_spacing") || is_string(key,"subsong_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->subsong_spacing)) goto fail;
    }
    else if (is_string(key,"name_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->name_offset)) goto fail;
        txth->name_offset_set = 1;
        /* special adjustment */
        txth->name_offset += txth->base_offset;
        if (txth->subsong_spacing && !txth->is_offset_absolute)
            txth->name_offset += txth->subsong_spacing * (txth->target_subsong - 1);
    }
    else if (is_string(key,"name_offset_absolute")) { //TODO: remove
        if (!parse_num(txth->sf_head,txth,val, &txth->name_offset)) goto fail;
        txth->name_offset_set = 1;
        /* special adjustment */
        txth->name_offset += txth->base_offset;
        /* unlike the above this is meant for reads that point to somewhere in the file, regardless subsong number */
    }
    else if (is_string(key,"name_size")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->name_size)) goto fail;
    }

    /* SUBFILES */
    else if (is_string(key,"subfile_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->subfile_offset)) goto fail;
        txth->subfile_set = 1;
    }
    else if (is_string(key,"subfile_size")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->subfile_size)) goto fail;
        txth->subfile_set = 1;
    }
    else if (is_string(key,"subfile_extension")) {
        if (!parse_string(txth->sf_head,txth,val, txth->subfile_extension)) goto fail;
        txth->subfile_set = 1;
    }

    /* HEADER/BODY CONFIG */
    else if (is_string(key,"header_file")) {

        /* first remove old head if needed */
        if (txth->sf_head_opened) {
            close_streamfile(txth->sf_head);
            txth->sf_head = NULL;
            txth->sf_head_opened = 0;
        }

        if (is_string(val,"null")) { /* reset */
            if (!txth->streamfile_is_txth) {
                txth->sf_head = txth->sf; /* base non-txth file */
            }
            else {
                goto fail; /* .txth, nothing to fall back */
            }
        }
        else if (val[0]=='*' && val[1]=='.') { /* basename + extension */
            txth->sf_head = open_streamfile_by_ext(txth->sf, (val+2));
            if (!txth->sf_head) goto fail;
            txth->sf_head_opened = 1;
        }
        else { /* open file */
            fix_dir_separators(val); /* clean paths */

            txth->sf_head = open_streamfile_by_filename(txth->sf, val);
            if (!txth->sf_head) goto fail;
            txth->sf_head_opened = 1;
        }
    }
    else if (is_string(key,"body_file")) {

        /* first remove old body if needed */
        if (txth->sf_body_opened) {
            close_streamfile(txth->sf_body);
            txth->sf_body = NULL;
            txth->sf_body_opened = 0;
        }

        if (is_string(val,"null")) { /* reset */
            if (!txth->streamfile_is_txth) {
                txth->sf_body = txth->sf; /* base non-txth file */
            }
            else {
                goto fail; /* .txth, nothing to fall back */
            }
        }
        else if (val[0]=='*' && val[1]=='.') { /* basename + extension */
            txth->sf_body = open_streamfile_by_ext(txth->sf, (val+2));
            if (!txth->sf_body) goto fail;
            txth->sf_body_opened = 1;
        }
        else { /* open file */
            fix_dir_separators(val); /* clean paths */

            txth->sf_body = open_streamfile_by_filename(txth->sf, val);
            if (!txth->sf_body) goto fail;
            txth->sf_body_opened = 1;
        }

        /* use body as header when opening a .txth directly to simplify things */
        if (txth->streamfile_is_txth && !txth->sf_head_opened) {
            txth->sf_head = txth->sf_body;
        }

        /* re-apply */
        if (!txth->data_size_set) {
            txth->data_size = get_streamfile_size(txth->sf_body);

            /* maybe should be manually set again? */
            if (txth->data_size && txth->data_size > txth->next_offset && txth->next_offset)
                txth->data_size = txth->next_offset;
            if (txth->data_size && txth->data_size > txth->start_offset)
                txth->data_size -= txth->start_offset;
            if (txth->data_size && txth->data_size > txth->padding_size)
                txth->data_size -= txth->padding_size;
        }
    }

    /* CHUNKS */
    else if (is_string(key,"chunk_count")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_count)) goto fail;
        txth->chunk_count_set = 1;
        set_body_chunk(txth);
    }
    else if (is_string(key,"chunk_start")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_start)) goto fail;
        txth->chunk_start_set = 1;
        set_body_chunk(txth);
    }
    else if (is_string(key,"chunk_size")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_size)) goto fail;
        txth->chunk_size_set = 1;
        set_body_chunk(txth);
    }
    /* optional and should go before the above */
    else if (is_string(key,"chunk_number")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_number)) goto fail;
    }
    else if (is_string(key,"chunk_header_size")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_header_size)) goto fail;
    }
    else if (is_string(key,"chunk_data_size")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_data_size)) goto fail;
    }
    else if (is_string(key,"chunk_value")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_value)) goto fail;
    }
    else if (is_string(key,"chunk_size_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->chunk_size_offset)) goto fail;
    }
    else if (is_string(key,"chunk_endianness")) {
        if (!parse_endianness(txth, val, &txth->chunk_be, NULL)) goto fail;
    }


    /* BASE OFFSET */
    else if (is_string(key,"base_offset")) {
        if (!parse_num(txth->sf_head,txth,val, &txth->base_offset)) goto fail;
    }

    /* NAME TABLE */
    else if (is_string(key,"name_table")) {
        if (!parse_name_table(txth,val)) goto fail;
    }

    /* MULTI TXTH */
    else if (is_string(key,"multi_txth")) {
        if (!parse_multi_txth(txth,val)) goto fail;
    }

    /* DEBUG */
    else if (is_string(key,"debug")) {
        txth->debug = 1;
    }

    /* DEFAULT */
    else {
        VGM_LOG("TXTH: unknown key=%s, val=%s\n", key,val);
        goto fail;
    }

    //;VGM_LOG("TXTH: data_size=%x, start=%x, next=%x, padding=%x\n", txth->data_size, txth->start_offset, txth->next_offset, txth->padding_size);

    return 1;
fail:
    vgm_logi("TXTH: error parsing key=%s, val=%s\n", key, val);
    return 0;
}

static int is_substring(const char* val, const char* cmp, int inline_field) {
    char chr;
    int len = strlen(cmp);
    /* "val" must contain "cmp" entirely */
    if (strncmp(val, cmp, len) != 0)
        return 0;

    chr = val[len];

    /* "val" can end with math for inline fields (like interleave*0x10) */
    if (inline_field && (chr == '+' || chr == '-' || chr == '*' || chr == '/' || chr == '&'))
        return len;

    /* otherwise "val" ends in space or eof (to tell "interleave" and "interleave_last" apart) */
    if (chr != '\0' && chr != ' ')
        return 0;

    return len;
}

static int is_string(const char* val, const char* cmp) {
    int len = is_substring(val, cmp, 0);
    if (!len) return 0;

    /* also test that after string there aren't other values
     * (comments are already removed but trailing spaces are allowed) */
    while (val[len] != '\0') {
        if (val[len] != ' ')
            return 0;
        len++;
    }

    return len;
}
static int is_string_field(const char* val, const char* cmp) {
    return is_substring(val, cmp, 1);
}

static uint16_t get_string_wchar(const char* val, int pos, int* csize) {
    uint16_t wchar = 0;

    if ((val[pos] & 0x80) && val[pos+1] != '\0') {
        wchar = (((val[pos] << 8u) & 0xFF00) | (val[pos+1] & 0xFF));
        //wchar = ((((uint16_t)val[pos] << 8u)) | ((uint16_t)val[pos+1]));
        if (csize) *csize = 2;

        if (wchar >= 0xc380 && wchar <= 0xc39f) /* ghetto lowercase for common letters */
            wchar += 0x20;
    } else {
        wchar = val[pos];
        if (csize) *csize = 1;

        if (wchar >= 0x41 && wchar <= 0x5a)
            wchar += 0x20;
        if (wchar == '\\')
            wchar = '/'; /* normalize paths */
    }

    return wchar;
}
static int is_string_match(const char* text, const char* pattern) {
    int t_pos = 0, p_pos = 0, t_len = 0, p_len = 0;
    int p_size, t_size;
    uint16_t p_char, t_char;

    //;VGM_LOG("TXTH: match '%s' vs '%s'\n", text,pattern);

    /* compares 2 strings (case insensitive, to a point) allowing wildcards
     * ex. for "test": match = "Test*", "*est", "*teSt","T*ES*T"; fail = "tst", "teest"
     *
     * does some bleh UTF-8 handling, consuming dual bytes if needed (codepages set char's eighth bit).
     * as such it's slower than standard funcs, but it's not like we need it to be ultra fast.
     */

    while (text[t_pos] != '\0' && pattern[p_pos] != '\0') {
        //;VGM_LOG("TXTH:  compare '%s' vs '%s'\n", (text+t_pos), (pattern+p_pos));

        if (pattern[p_pos] == '*') {
            /* consume text wchars until one matches next pattern char */
            p_pos++;
            p_char = get_string_wchar(pattern, p_pos, NULL); /* stop char, or null */

            while(text[t_pos] != '\0') {
                t_char = get_string_wchar(text, t_pos, &t_size);
                //;VGM_LOG("TXTH:  consume %i '%s'\n", t_size, (text+t_pos));

                /* break from this wildcard (AKA possible match = stop consuming) only if:
                 * - rest of string has the same length (=could be a match)
                 * - there are more wildcards (=would consume other parts)
                 * otherwise current wildcard must keep consuming text (without this,
                 * sound_1_1.adx vs *_1.adx wouldn't match since the _ would stop too early)
                 */
                if (t_char == p_char) {
                    if (strchr(&pattern[p_pos], '*'))
                        break;

                    if (!t_len || !p_len) { /* lazy init helpful? */
                        t_len = strlen(text);
                        p_len = strlen(pattern);
                    }

                    //;VGM_LOG("TXTH:  possible match '%s' vs '%s'\n", (text+t_pos), (pattern+p_pos));
                    /* not strcmp to allow case insensitive-ness, handled below */
                    if (t_len - t_pos == p_len - p_pos)
                        break;
                }
                t_pos += t_size;
            }
        }
        else if (pattern[p_pos] == '?') {
            /* skip next text wchar */
            get_string_wchar(text, t_pos, &t_size);
            p_pos++;
            t_pos += t_size;
        }
        else { /* must match char 1:1 */
            //;VGM_LOG("TXTH:  test 1:1 '%s' vs '%s'\n", (text+t_pos), (pattern+p_pos));
            t_char = get_string_wchar(text, t_pos, &t_size);
            p_char = get_string_wchar(pattern, p_pos, &p_size);
            if (p_char != t_char)
                break;
            t_pos += t_size;
            p_pos += p_size;
        }
    }

    //;VGM_LOG("TXTH:  current '%s' vs '%s'\n", (text+t_pos), (pattern+p_pos));
    //;VGM_LOG("TXTH: match '%s' vs '%s' = %s\n", text,pattern, (text[t_pos] == '\0' && pattern[p_pos] == '\0') ? "true" : "false");

    /* either all chars consumed/matched and both pos point to null, or one didn't so string didn't match */
    return text[t_pos] == '\0' && pattern[p_pos] == '\0';
}
static int parse_string(STREAMFILE* sf, txth_header* txth, const char* val, char* str) {
    int n = 0;

    /* read string without trailing spaces */
    if (sscanf(val, " %s%n[^ ]%n", str, &n, &n) != 1)
        return 0;
    return n;
}

static int parse_coef_table(STREAMFILE* sf, txth_header* txth, const char* val, uint8_t* out_value, size_t out_size) {
    uint32_t byte;
    int done = 0;

    /* read 2 char pairs = 1 byte ('N' 'N' 'M' 'M' = 0xNN 0xMM )*/
    while (val[0] != '\0') {
        if (val[0] == ' ') {
            val++;
            continue;
        }

        if (val[0] == '0' && val[1] == 'x')  /* allow "0x" before values */
            val += 2;
        if (sscanf(val, " %2x", &byte) != 1)
            goto fail;
        if (done + 1 >= out_size)
            goto fail;

        out_value[done] = (uint8_t)byte;
        done++;
        val += 2;
    }

    return 1;
fail:
    return 0;
}

static void string_trim(char* str) {
    int str_len = strlen(str);
    int i;
    for (i = str_len - 1; i >= 0; i--) {
        if (str[i] != ' ')
            break;
        str[i] = '\0';
    }
}

static int read_name_table_keyval(txth_header* txth, const char* line, char* key, char* val) {
    int ok;
    int subsong;

    /* get key/val (ignores lead spaces, stops at space/comment/separator) */
    //todo names with # and subsongs don't work

    /* ignore comments (that aren't subsongs) */
    if (line[0] == '#' && strchr(line,':') == NULL)
        return 0;

    /* try "(name): (val))" */
    
    ok = sscanf(line, " %[^\t#:] : %[^\t#\r\n] ", key, val);
    if (ok == 2) {
        string_trim(key); /* otherwise includes end spaces before : */
        //;VGM_LOG("TXTH: name %s get\n", key);
        return 1;
    }

    /* try "(empty): (val))" */
    key[0] = '\0';
    ok = sscanf(line, " : %[^\t#\r\n] ", val);
    if (ok == 1) {
        //;VGM_LOG("TXTH: default get\n");
        return 1;
    }

    /* try "(name)#subsong: (val))" */
    ok = sscanf(line, " %[^\t#:]#%i : %[^\t#\r\n] ", key, &subsong, val);
    if (ok == 3 && subsong == txth->target_subsong) {
        //;VGM_LOG("TXTH: name %s + subsong %i get\n", key, subsong);
        return 1;
    }

    /* try "(empty)#subsong: (val))" */
    key[0] = '\0';
    ok = sscanf(line, " #%i: %[^\t#\r\n] ", &subsong, val);
    if (ok == 2 && subsong == txth->target_subsong) {
        //;VGM_LOG("TXTH: default + subsong %i get\n", subsong);
        return 1;
    }

    return 0;
}

static int parse_name_val(txth_header* txth, char* subval) {
    int ok;

    ok = parse_num(txth->sf_head, txth, subval, &txth->name_values[txth->name_values_count]);
    if (!ok) {
        /* in rare cases may set codec */
        txth_codec_t codec = parse_codec(txth, subval);
        if (codec == UNKNOWN)
            goto fail;
        txth->name_values[txth->name_values_count] = codec;
    }
    txth->name_values_count++;
    if (txth->name_values_count >= 16) /* surely nobody needs that many */
        goto fail;

    return 1;
fail:
    return 0;
}

static int parse_name_table(txth_header* txth, char* set_name) {
    STREAMFILE* sf_names = NULL;
    off_t txt_offset, file_size;
    char fullname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    char basename[PATH_LIMIT];
    const char* table_name;

    /* just in case */
    if (!txth->sf_text || !txth->sf_body)
        goto fail;

    /* trim just in case */
    string_trim(set_name);
    if (is_string(set_name,"*"))
        table_name = ".names.txt";
    else
        table_name = set_name;

    /* open companion file near .txth */
    sf_names = open_streamfile_by_filename(txth->sf_text, table_name);
    if (!sf_names) goto fail;

    get_streamfile_name(txth->sf_body, fullname, sizeof(filename));
    get_streamfile_filename(txth->sf_body, filename, sizeof(filename));
    get_streamfile_basename(txth->sf_body, basename, sizeof(basename));
    //;VGM_LOG("TXTH: names full=%s, file=%s, base=%s\n", fullname, filename, basename);

    txt_offset = read_bom(sf_names);
    file_size = get_streamfile_size(sf_names);

    /* in case of repeated name tables */
    memset(txth->name_values, 0, sizeof(txth->name_values));
    txth->name_values_count = 0;

    /* read lines and find target filename, format is (filename): value1, ... valueN */
    {
        char line[TXT_LINE_MAX];
        char key[TXT_LINE_MAX];
        char val[TXT_LINE_MAX];

        while (txt_offset < file_size) {
            int ok, bytes_read, line_ok;

            bytes_read = read_line(line, sizeof(line), txt_offset, sf_names, &line_ok);
            if (!line_ok) goto fail;
            //;VGM_LOG("TXTH: line=%s\n",line);

            txt_offset += bytes_read;

            if (!read_name_table_keyval(txth, line, key, val))
                continue;

            //;VGM_LOG("TXTH: compare name '%s'\n", key);
            /* parse values if key (name) matches default ("") or filename with/without extension */
            if (key[0]=='\0'
                    || is_string_match(filename, key)
                    || is_string_match(basename, key)
                    || is_string_match(fullname, key)) {
                int n;
                char subval[TXT_LINE_MAX];
                const char *current = val;

                while (current[0] != '\0') {
                    ok = sscanf(current, " %[^\t#\r\n,]%n ", subval, &n);
                    if (ok != 1)
                        goto fail;

                    current += n;
                    if (current[0] == ',')
                        current++;

                    if (!parse_name_val(txth, subval))
                        goto fail;
                }

                //;VGM_LOG("TXTH: found name '%s'\n", key);
                break; /* target found */
            }
        }
    }

    /* ignore if name is not actually found (values will return 0) */

    close_streamfile(sf_names);
    return 1;
fail:
    close_streamfile(sf_names);
    return 0;
}


static int parse_multi_txth(txth_header* txth, char* names) {
    STREAMFILE* sf_text = NULL;
    char name[PATH_LIMIT];
    int n, ok;

    /* temp save */
    sf_text = txth->sf_text;
    txth->sf_text = NULL;

    /* to avoid potential infinite recursion plus stack overflows */
    if (txth->is_multi_txth > 3)
        goto fail;
    txth->is_multi_txth++;

    while (names[0] != '\0') {
        STREAMFILE* sf_test = NULL;
        int found;

        ok = sscanf(names, " %[^\t#\r\n,]%n ", name, &n);
        if (ok != 1)
            goto fail;

        //;VGM_LOG("TXTH: multi name %s\n", name);
        sf_test = open_streamfile_by_filename(txth->sf, name);
        if (!sf_test)
            goto fail;

        /* re-parse with current txth and hope */
        txth->sf_text = sf_test;
        found = parse_txth(txth);
        close_streamfile(sf_test);
        //todo may need to close header/body streamfiles?

        if (found) {
            //;VGM_LOG("TXTH: found valid multi txth %s\n", name);
            break; /* found, otherwise keep trying */
        }

        names += n;
        if (names[0] == ',')
            names++;
    }

    txth->is_multi_txth--;
    txth->sf_text = sf_text;
    return 1;
fail:
    txth->is_multi_txth--;
    txth->sf_text = sf_text;
    return 0;
}

static int parse_num(STREAMFILE* sf, txth_header* txth, const char* val, uint32_t* out_value) {
    /* out_value can be these, save before modifying */
    uint32_t value_mul = txth->value_mul;
    uint32_t value_div = txth->value_div;
    uint32_t value_add = txth->value_add;
    uint32_t value_sub = txth->value_sub;
    uint32_t subsong_spacing = txth->subsong_spacing;

    char op = ' ';
    int brackets = 0;
    uint32_t result = 0;

    //;VGM_LOG("TXTH: initial val '%s'\n", val);


    /* read "val" format: @(offset) (op) (field) (op) (number) ... */
    while (val[0] != '\0') {
        uint32_t value = 0;
        char type = val[0];
        int value_read = 0;
        int n = 0;

        if (type == ' ') { /* ignore */
            n = 1;
        }
        else if (type == '(') { /* bracket */
            brackets++;
            n = 1;
        }
        else if (type == ')') { /* bracket */
            if (brackets == 0) goto fail;
            brackets--;
            n = 1;
        }
        else if (type == '+' || type == '-' || type == '/' || type == '*' || type == '&') { /* op */
            op = type;
            n = 1;
        }
        else if (type == '@') { /* offset */
            uint32_t offset = 0;
            char ed1 = 'L', ed2 = 'E';
            int size = 4;
            int big_endian = 0;
            int hex = (val[1]=='0' && val[2]=='x');

            /* can happen when loading .txth and not setting body/head */
            if (!sf) {
                VGM_LOG("TXTH: wrong header\n");
                goto fail;
            }

            /* read exactly N fields in the expected format */
            if (strchr(val,':') && strchr(val,'$')) {
                if (sscanf(val, hex ? "@%x:%c%c$%i%n" : "@%u:%c%c$%i%n", &offset, &ed1,&ed2, &size, &n) != 4) goto fail;
            } else if (strchr(val,':')) {
                if (sscanf(val, hex ? "@%x:%c%c%n" : "@%u:%c%c%n", &offset, &ed1,&ed2, &n) != 3) goto fail;
            } else if (strchr(val,'$')) {
                if (sscanf(val, hex ? "@%x$%i%n" : "@%u$%i%n", &offset, &size, &n) != 2) goto fail;
            } else {
                if (sscanf(val, hex ? "@%x%n" : "@%u%n", &offset, &n) != 1) goto fail;
            }

            /* adjust offset */
            offset += txth->base_offset;

            if (/*offset < 0 ||*/ offset > get_streamfile_size(sf)) {
                vgm_logi("TXTH: wrong offset over file size (%x + %x)\n", offset - txth->base_offset, txth->base_offset);
                goto fail;
            }

            if (ed1 == 'B' && ed2 == 'E')
                big_endian = 1;
            else if (!(ed1 == 'L' && ed2 == 'E'))
                goto fail;

            if (subsong_spacing)
                offset = offset + subsong_spacing * (txth->target_subsong - 1);

            if (txth->debug)
                vgm_logi("TXTH: use value at 0x%x (%s %ib)\n", offset, big_endian ? "BE" : "LE", size * 8);

            switch(size) {
                case 1: value = read_u8(offset,sf); break;
                case 2: value = big_endian ? read_u16be(offset,sf) : read_u16le(offset,sf); break;
                case 3: value = (big_endian ? read_u32be(offset,sf) : read_u32le(offset,sf)) & 0x00FFFFFF; break;
                case 4: value = big_endian ? read_u32be(offset,sf) : read_u32le(offset,sf); break;
                default: goto fail;
            }
            value_read = 1;
        }
        else if (type >= '0' && type <= '9') { /* unsigned constant */
            int hex = (val[0]=='0' && val[1]=='x');

            if (sscanf(val, hex ? "%x%n" : "%u%n", &value, &n) != 1)
                goto fail;
            value_read = 1;

            if (txth->debug)
                vgm_logi(hex ? "TXTH: use constant 0x%x\n" : "TXTH: use constant %i\n", value);
        }
        else { /* known field */
            if      ((n = is_string_field(val,"interleave")))           value = txth->interleave;
            else if ((n = is_string_field(val,"interleave_last")))      value = txth->interleave_last;
            else if ((n = is_string_field(val,"interleave_first")))     value = txth->interleave_first;
            else if ((n = is_string_field(val,"interleave_first_skip")))value = txth->interleave_first_skip;
            else if ((n = is_string_field(val,"channels")))             value = txth->channels;
            else if ((n = is_string_field(val,"sample_rate")))          value = txth->sample_rate;
            else if ((n = is_string_field(val,"start_offset")))         value = txth->start_offset;
            else if ((n = is_string_field(val,"data_size")))            value = txth->data_size;
            else if ((n = is_string_field(val,"padding_size")))         value = txth->padding_size;
            else if ((n = is_string_field(val,"num_samples")))          value = txth->num_samples;
            else if ((n = is_string_field(val,"loop_start_sample")))    value = txth->loop_start_sample;
            else if ((n = is_string_field(val,"loop_start")))           value = txth->loop_start_sample;
            else if ((n = is_string_field(val,"loop_end_sample")))      value = txth->loop_end_sample;
            else if ((n = is_string_field(val,"loop_end")))             value = txth->loop_end_sample;
            else if ((n = is_string_field(val,"subsong")))              value = txth->target_subsong;
            else if ((n = is_string_field(val,"subsong_count")))        value = txth->subsong_count;
            else if ((n = is_string_field(val,"subsong_spacing")))      value = txth->subsong_spacing;
            else if ((n = is_string_field(val,"subsong_offset")))       value = txth->subsong_spacing;
            else if ((n = is_string_field(val,"subfile_offset")))       value = txth->subfile_offset;
            else if ((n = is_string_field(val,"subfile_size")))         value = txth->subfile_size;
            else if ((n = is_string_field(val,"base_offset")))          value = txth->base_offset;
            else if ((n = is_string_field(val,"coef_offset")))          value = txth->coef_offset;
            else if ((n = is_string_field(val,"coef_spacing")))         value = txth->coef_spacing;
            else if ((n = is_string_field(val,"hist_offset")))          value = txth->hist_offset;
            else if ((n = is_string_field(val,"hist_spacing")))         value = txth->hist_spacing;
            else if ((n = is_string_field(val,"chunk_count")))          value = txth->chunk_count;
            else if ((n = is_string_field(val,"chunk_start")))          value = txth->chunk_start;
            else if ((n = is_string_field(val,"chunk_size")))           value = txth->chunk_size;
            else if ((n = is_string_field(val,"chunk_size_offset")))    value = txth->chunk_size_offset;
            else if ((n = is_string_field(val,"chunk_number")))         value = txth->chunk_number;
            else if ((n = is_string_field(val,"chunk_data_size")))      value = txth->chunk_data_size;
            else if ((n = is_string_field(val,"chunk_header_size")))    value = txth->chunk_header_size;
            //todo whatever, improve
            else if ((n = is_string_field(val,"name_value")))           value = txth->name_values[0];
            else if ((n = is_string_field(val,"name_value1")))          value = txth->name_values[0];
            else if ((n = is_string_field(val,"name_value2")))          value = txth->name_values[1];
            else if ((n = is_string_field(val,"name_value3")))          value = txth->name_values[2];
            else if ((n = is_string_field(val,"name_value4")))          value = txth->name_values[3];
            else if ((n = is_string_field(val,"name_value5")))          value = txth->name_values[4];
            else if ((n = is_string_field(val,"name_value6")))          value = txth->name_values[5];
            else if ((n = is_string_field(val,"name_value7")))          value = txth->name_values[6];
            else if ((n = is_string_field(val,"name_value8")))          value = txth->name_values[7];
            else if ((n = is_string_field(val,"name_value9")))          value = txth->name_values[8];
            else if ((n = is_string_field(val,"name_value10")))         value = txth->name_values[9];
            else if ((n = is_string_field(val,"name_value11")))         value = txth->name_values[10];
            else if ((n = is_string_field(val,"name_value12")))         value = txth->name_values[11];
            else if ((n = is_string_field(val,"name_value13")))         value = txth->name_values[12];
            else if ((n = is_string_field(val,"name_value14")))         value = txth->name_values[13];
            else if ((n = is_string_field(val,"name_value15")))         value = txth->name_values[14];
            else if ((n = is_string_field(val,"name_value16")))         value = txth->name_values[15];
            else goto fail;
            value_read = 1;

            if (txth->debug)
                vgm_logi("TXTH: use field value 0x%x\n", value);
        }

        /* apply simple left-to-right math though, for now "(" ")" are counted and validated
         * (could use good ol' shunting-yard algo but whatevs) */
        if (value_read) {
            //;VGM_ASSERT(op != ' ', "MIX: %i %c %i\n", result, op, value);
            switch(op) {
                case '+': value = result + value; break;
                case '-': value = result - value; break;
                case '*': value = result * value; break;
                case '/': if (value == 0) goto fail; value = result / value; break;
                case '&': value = result & value; break;
                default: break;
            }
            op = ' '; /* consume */

            result = value;
        }

        /* move to next field (if any) */
        val += n;

        //;VGM_LOG("TXTH: val='%s', n=%i, brackets=%i, result=0x%x\n", val, n, brackets, result);
    }

    /* unbalanced brackets */
    if (brackets > 0)
        goto fail;

    /* global operators, but only if current value wasn't set to 0 right before */
    if (value_mul && txth->value_mul)
        result = result * value_mul;
    if (value_div && txth->value_div)
        result = result / value_div;
    if (value_add && txth->value_add)
        result = result + value_add;
    if (value_sub && txth->value_sub)
        result = result - value_sub;

    *out_value = result;

    if (txth->debug)
        vgm_logi("TXTH: final value: %u (0x%x)\n", result, result);

    return 1;
fail:
    if (txth->debug)
        vgm_logi("TXTH: error parsing num '%s'\n", val);
    return 0;
}

static int get_bytes_to_samples(txth_header* txth, uint32_t bytes) {
    switch(txth->codec) {
        case MS_IMA:
            return ms_ima_bytes_to_samples(bytes, txth->interleave, txth->channels);
        case XBOX:
            return xbox_ima_bytes_to_samples(bytes, txth->channels);
        case NGC_DSP:
            return dsp_bytes_to_samples(bytes, txth->channels);
        case PSX:
        case PSX_bf:
        case HEVAG:
            return ps_bytes_to_samples(bytes, txth->channels);
        case PCM16BE:
        case PCM16LE:
            return pcm16_bytes_to_samples(bytes, txth->channels);
        case PCM8:
        case PCM8_U_int:
        case PCM8_U:
        case PCM8_SB:
            return pcm8_bytes_to_samples(bytes, txth->channels);
        case PCM_FLOAT_LE:
            return pcm_bytes_to_samples(bytes, txth->channels, 32);
        case PCM4:
        case PCM4_U:
        case TGC:
            return pcm_bytes_to_samples(bytes, txth->channels, 4);
        case MSADPCM:
            return msadpcm_bytes_to_samples(bytes, txth->interleave, txth->channels);
        case ATRAC3:
            return atrac3_bytes_to_samples(bytes, txth->interleave);
        case ATRAC3PLUS:
            return atrac3plus_bytes_to_samples(bytes, txth->interleave);
        case AAC:
            return aac_get_samples(txth->sf_body, txth->start_offset, bytes);
#ifdef VGM_USE_MPEG
        case MPEG:
            return mpeg_get_samples(txth->sf_body, txth->start_offset, bytes);
#endif
        case AC3:
            return ac3_bytes_to_samples(bytes, txth->interleave, txth->channels);
        case ASF:
            return asf_bytes_to_samples(bytes, txth->channels);
        case EAXA:
            return ea_xa_bytes_to_samples(bytes, txth->channels);
        case XA:
        case XA_EA:
            return xa_bytes_to_samples(bytes, txth->channels, 0, 0, 4);

        /* XMA bytes-to-samples is done at the end as the value meanings are a bit different */
        case XMA1:
        case XMA2:
            return bytes; /* preserve */

        case IMA:
        case DVI_IMA:
        case IMA_HV:
            return ima_bytes_to_samples(bytes, txth->channels);
        case AICA:
        case YMZ:
        case CP_YM:
            return yamaha_bytes_to_samples(bytes, txth->channels);
        case PCFX:
        case OKI16:
        case OKI4S:
            return oki_bytes_to_samples(bytes, txth->channels);

        /* untested */
        case SDX2:
            return bytes;
        case NGC_DTK:
            return bytes / 0x20 * 28; /* always stereo */
        case APPLE_IMA4:
            if (!txth->interleave) return 0;
            return (bytes / txth->interleave) * (txth->interleave - 2) * 2;

        case FFMPEG: /* too complex, try after init */
        default:
            return 0;
    }
}

static int get_padding_size(txth_header* txth, int discard_empty) {
    if (txth->data_size == 0 || txth->channels == 0)
        return 0;

    switch(txth->codec) {
        case PSX:
            return ps_find_padding(txth->sf_body, txth->start_offset, txth->data_size, txth->channels, txth->interleave, discard_empty);
        default:
            return 0;
    }
}
