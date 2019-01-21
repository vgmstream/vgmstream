#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

#define TXT_LINE_MAX 0x2000

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
    AICA = 10,          /* AICA ADPCM (Dreamcast games) */
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
} txth_type;

typedef struct {
    txth_type codec;
    uint32_t codec_mode;

    uint32_t value_mul;
    uint32_t value_div;
    uint32_t value_add;
    uint32_t value_sub;

    uint32_t id_value;
    uint32_t id_offset;

    uint32_t interleave;
    uint32_t interleave_last;
    uint32_t channels;
    uint32_t sample_rate;

    uint32_t data_size;
    int data_size_set;
    uint32_t start_offset;

    int sample_type;
    uint32_t num_samples;
    uint32_t loop_start_sample;
    uint32_t loop_end_sample;
    uint32_t loop_adjust;
    int skip_samples_set;
    uint32_t skip_samples;

    uint32_t loop_flag;
    int loop_flag_set;
    int loop_flag_auto;

    uint32_t coef_offset;
    uint32_t coef_spacing;
    uint32_t coef_big_endian;
    uint32_t coef_mode;

    int num_samples_data_size;

    int target_subsong;
    uint32_t subsong_count;
    uint32_t subsong_offset;

    uint32_t name_offset_set;
    uint32_t name_offset;
    uint32_t name_size;

    /* original STREAMFILE and its type (may be an unsupported "base" file or a .txth) */
    STREAMFILE *streamFile;
    int streamfile_is_txth;

    /* configurable STREAMFILEs and if we opened it (thus must close it later) */
    STREAMFILE *streamText;
    STREAMFILE *streamHead;
    STREAMFILE *streamBody;
    int streamtext_opened;
    int streamhead_opened;
    int streambody_opened;

} txth_header;


static STREAMFILE * open_txth(STREAMFILE * streamFile);
static int parse_txth(txth_header * txth);
static int parse_keyval(STREAMFILE * streamFile, txth_header * txth, const char * key, char * val);
static int parse_num(STREAMFILE * streamFile, txth_header * txth, const char * val, uint32_t * out_value);
static int get_bytes_to_samples(txth_header * txth, uint32_t bytes);


/* TXTH - an artificial "generic" header for headerless streams.
 * Similar to GENH, but with a single separate .txth file in the dir and text-based. */
VGMSTREAM * init_vgmstream_txth(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    txth_header txth = {0};
    coding_t coding;
    int i, j;


    /* accept .txth (should set body_file or will fail later) */
    if (check_extensions(streamFile, "txth")) {
        txth.streamFile = streamFile;
        txth.streamfile_is_txth = 1;

        txth.streamText = streamFile;
        txth.streamHead = NULL;
        txth.streamBody = NULL;
        txth.streamtext_opened = 0;
        txth.streamhead_opened = 0;
        txth.streambody_opened = 0;
    }
    else {
        /* accept base file (no need for ID or ext checks --if a companion .TXTH exists all is good)
         * (player still needs to accept the streamfile's ext, so at worst rename to .vgmstream) */
        STREAMFILE * streamText = open_txth(streamFile);
        if (!streamText) goto fail;

        txth.streamFile = streamFile;
        txth.streamfile_is_txth = 0;

        txth.streamText = streamText;
        txth.streamHead = streamFile;
        txth.streamBody = streamFile;
        txth.streamtext_opened = 1;
        txth.streamhead_opened = 0;
        txth.streambody_opened = 0;
    }

    /* process the text file */
    if (!parse_txth(&txth))
        goto fail;


    /* type to coding conversion */
    switch (txth.codec) {
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
        case FFMPEG:     coding = coding_FFmpeg; break;
#endif
        case PCFX:       coding = coding_PCFX; break;
        case PCM4:       coding = coding_PCM4; break;
        case PCM4_U:     coding = coding_PCM4_U; break;
        case OKI16:      coding = coding_OKI16; break;
        default:
            goto fail;
    }


    /* try to autodetect PS-ADPCM loop data */
    if (txth.loop_flag_auto && coding == coding_PSX) {
        txth.loop_flag = ps_find_loop_offsets(txth.streamBody, txth.start_offset, txth.data_size, txth.channels, txth.interleave,
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
        read_string(vgmstream->stream_name,name_size, txth.name_offset,txth.streamHead);
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
        case coding_PCM4:
        case coding_PCM4_U:
        case coding_SDX2:
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_DVI_IMA:
        case coding_IMA:
        case coding_AICA:
        case coding_APPLE_IMA4:
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
                        coding == coding_IMA_int ||
                        coding == coding_DVI_IMA_int ||
                        coding == coding_SDX2_int ||
                        coding == coding_AICA_int) ) {
                    goto fail;
                }
            } else {
                vgmstream->layout_type = layout_none;
            }

            /* setup adpcm */
            if (coding == coding_AICA || coding == coding_AICA_int) {
                int i;
                for (i=0;i<vgmstream->channels;i++) {
                    vgmstream->ch[i].adpcm_step_index = 0x7f;
                }
            }

            if (coding == coding_PCM4 || coding == coding_PCM4_U) {
                /* high nibble or low nibble first */
                vgmstream->codec_config = txth.codec_mode;
            }
            break;

        case coding_PCFX:
            vgmstream->interleave_block_size = txth.interleave;
            vgmstream->interleave_last_block_size = txth.interleave_last;
            vgmstream->layout_type = layout_interleave;
            if (txth.codec_mode >= 0 && txth.codec_mode <= 3)
                vgmstream->codec_config = txth.codec_mode;
            break;

        case coding_OKI16:
            vgmstream->layout_type = layout_none;
            break;

        case coding_MS_IMA:
            if (!txth.interleave) goto fail; /* creates garbage */

            vgmstream->interleave_block_size = txth.interleave;
            vgmstream->layout_type = layout_none;
            break;
        case coding_MSADPCM:
            if (vgmstream->channels > 2) goto fail;
            if (!txth.interleave) goto fail; /* creates garbage */

            vgmstream->interleave_block_size = txth.interleave;
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
            for (i=0;i<vgmstream->channels;i++) {
                int16_t (*read_16bit)(off_t , STREAMFILE*) = txth.coef_big_endian ? read_16bitBE : read_16bitLE;

                /* normal/split coefs */
                if (txth.coef_mode == 0) { /* normal mode */
                    for (j = 0; j < 16; j++) {
                        vgmstream->ch[i].adpcm_coef[j] = read_16bit(txth.coef_offset + i*txth.coef_spacing  + j*2, txth.streamHead);
                    }
                }
                else { /* split coefs */
                    goto fail; //IDK what is this
                    /*
                    for (j = 0; j < 8; j++) {
                        vgmstream->ch[i].adpcm_coef[j*2] = read_16bit(genh.coef_offset + i*genh.coef_spacing + j*2, txth.streamHead);
                        vgmstream->ch[i].adpcm_coef[j*2+1] = read_16bit(genh.coef_split_offset + i*genh.coef_split_spacing + j*2, txth.streamHead);
                    }
                    */
                }
            }

            break;
#ifdef VGM_USE_MPEG
        case coding_MPEG_layer3:
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_mpeg(txth.streamBody, txth.start_offset, &coding, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *ffmpeg_data = NULL;

            if (txth.codec == FFMPEG || txth.codec == AC3) {
                /* default FFmpeg */
                ffmpeg_data = init_ffmpeg_offset(txth.streamBody, txth.start_offset,txth.data_size);
                if ( !ffmpeg_data ) goto fail;

                if (vgmstream->num_samples == 0)
                    vgmstream->num_samples = ffmpeg_data->totalSamples; /* sometimes works */
            }
            else {
                /* fake header FFmpeg */
                uint8_t buf[200];
                int32_t bytes;

                if (txth.codec == ATRAC3) {
                    int block_size = txth.interleave;
                    int joint_stereo;
                    switch(txth.codec_mode) {
                        case 0: joint_stereo = vgmstream->channels > 1 && txth.interleave/vgmstream->channels==0x60 ? 1 : 0; break; /* autodetect */
                        case 1: joint_stereo = 1; break; /* force joint stereo */
                        case 2: joint_stereo = 0; break; /* force stereo */
                        default: goto fail;
                    }

                    bytes = ffmpeg_make_riff_atrac3(buf, 200, vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, txth.skip_samples);
                }
                else if (txth.codec == ATRAC3PLUS) {
                    int block_size = txth.interleave;

                    bytes = ffmpeg_make_riff_atrac3plus(buf, 200, vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, block_size, txth.skip_samples);
                }
                else if (txth.codec == XMA1) {
                    int xma_stream_mode = txth.codec_mode == 1 ? 1 : 0;

                    bytes = ffmpeg_make_riff_xma1(buf, 100, vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, xma_stream_mode);
                }
                else if (txth.codec == XMA2) {
                    int block_count, block_size;

                    block_size = txth.interleave ? txth.interleave : 2048;
                    block_count = txth.data_size / block_size;

                    bytes = ffmpeg_make_riff_xma2(buf, 200, vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
                }
                else {
                    goto fail;
                }

                ffmpeg_data = init_ffmpeg_header_offset(txth.streamBody, buf,bytes, txth.start_offset,txth.data_size);
                if ( !ffmpeg_data ) goto fail;
            }

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->layout_type = layout_none;

            if (txth.codec == XMA1 || txth.codec == XMA2) {
                xma_fix_raw_samples(vgmstream, txth.streamBody, txth.start_offset,txth.data_size, 0, 0,0);
            } else if (txth.skip_samples_set) { /* force encoder delay */
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

        xma_get_samples(&msd, txth.streamBody);

        vgmstream->num_samples = msd.num_samples;
        if (txth.sample_type==1) {
            vgmstream->loop_start_sample = msd.loop_start_sample;
            vgmstream->loop_end_sample = msd.loop_end_sample;
        }
    }
#endif

    vgmstream->coding_type = coding;
    vgmstream->meta_type = meta_TXTH;
    vgmstream->allow_dual_stereo = 1;


    if ( !vgmstream_open_stream(vgmstream,txth.streamBody,txth.start_offset) )
        goto fail;

    if (txth.streamtext_opened) close_streamfile(txth.streamText);
    if (txth.streamhead_opened) close_streamfile(txth.streamHead);
    if (txth.streambody_opened) close_streamfile(txth.streamBody);
    return vgmstream;

fail:
    if (txth.streamtext_opened) close_streamfile(txth.streamText);
    if (txth.streamhead_opened) close_streamfile(txth.streamHead);
    if (txth.streambody_opened) close_streamfile(txth.streamBody);
    close_vgmstream(vgmstream);
    return NULL;
}


static STREAMFILE * open_txth(STREAMFILE * streamFile) {
    char basename[PATH_LIMIT];
    char filename[PATH_LIMIT];
    char fileext[PATH_LIMIT];
    const char *subext;
    STREAMFILE * streamText;

    /* try "(path/)(name.ext).txth" */
    get_streamfile_name(streamFile,filename,PATH_LIMIT);
    strcat(filename, ".txth");
    streamText = open_streamfile(streamFile,filename);
    if (streamText) return streamText;

    /* try "(path/)(.sub.ext).txth" */
    get_streamfile_basename(streamFile,basename,PATH_LIMIT);
    subext = filename_extension(basename);
    if (subext != NULL) {
        get_streamfile_path(streamFile,filename,PATH_LIMIT);
        get_streamfile_ext(streamFile,fileext,PATH_LIMIT);
        strcat(filename,".");
        strcat(filename, subext);
        strcat(filename,".");
        strcat(filename, fileext);
        strcat(filename, ".txth");

        streamText = open_streamfile(streamFile,filename);
        if (streamText) return streamText;
    }

    /* try "(path/)(.ext).txth" */
    get_streamfile_path(streamFile,filename,PATH_LIMIT);
    get_streamfile_ext(streamFile,fileext,PATH_LIMIT);
    strcat(filename,".");
    strcat(filename, fileext);
    strcat(filename, ".txth");
    streamText = open_streamfile(streamFile,filename);
    if (streamText) return streamText;

    /* try "(path/).txth" */
    get_streamfile_path(streamFile,filename,PATH_LIMIT);
    strcat(filename, ".txth");
    streamText = open_streamfile(streamFile,filename);
    if (streamText) return streamText;

    /* not found */
    return NULL;
}

/* Simple text parser of "key = value" lines.
 * The code is meh and error handling not exactly the best. */
static int parse_txth(txth_header * txth) {
    off_t txt_offset = 0x00;
    off_t file_size = get_streamfile_size(txth->streamText);

    /* setup txth defaults */
    if (txth->streamBody)
        txth->data_size = get_streamfile_size(txth->streamBody);
    txth->target_subsong = txth->streamFile->stream_index;
    if (txth->target_subsong == 0) txth->target_subsong = 1;


    /* skip BOM if needed */
    if ((uint16_t)read_16bitLE(0x00, txth->streamText) == 0xFFFE ||
        (uint16_t)read_16bitLE(0x00, txth->streamText) == 0xFEFF) {
        txt_offset = 0x02;
    }
    else if (((uint32_t)read_32bitBE(0x00, txth->streamText) & 0xFFFFFF00) == 0xEFBBBF00) {
        txt_offset = 0x03;
    }

    /* read lines */
    while (txt_offset < file_size) {
        char line[TXT_LINE_MAX] = {0};
        char key[TXT_LINE_MAX] = {0}, val[TXT_LINE_MAX] = {0}; /* at least as big as a line to avoid overflows (I hope) */
        int ok, bytes_read, line_done;

        bytes_read = get_streamfile_text_line(TXT_LINE_MAX,line, txt_offset,txth->streamText, &line_done);
        if (!line_done) goto fail;
        //;VGM_LOG("TXTH: line=%s\n",line);

        txt_offset += bytes_read;
        
        /* get key/val (ignores lead/trail spaces, stops at space/comment/separator) */
        ok = sscanf(line, " %[^ \t#=] = %[^ \t#\r\n] ", key,val);
        if (ok != 2) /* ignore line if no key=val (comment or garbage) */
            continue;

        if (!parse_keyval(txth->streamFile, txth, key, val)) /* read key/val */
            goto fail;
    }

    if (!txth->loop_flag_set)
        txth->loop_flag = txth->loop_end_sample && txth->loop_end_sample != 0xFFFFFFFF;

    if (!txth->streamBody)
        goto fail;

    if (txth->data_size > get_streamfile_size(txth->streamBody) - txth->start_offset || txth->data_size == 0)
        txth->data_size = get_streamfile_size(txth->streamBody) - txth->start_offset;

    return 1;
fail:
    return 0;
}

static int parse_keyval(STREAMFILE * streamFile_, txth_header * txth, const char * key, char * val) {
    //;VGM_LOG("TXTH: key=%s, val=%s\n", key, val);

    if (0==strcmp(key,"codec")) {
        if      (0==strcmp(val,"PSX"))          txth->codec = PSX;
        else if (0==strcmp(val,"XBOX"))         txth->codec = XBOX;
        else if (0==strcmp(val,"NGC_DTK"))      txth->codec = NGC_DTK;
        else if (0==strcmp(val,"DTK"))          txth->codec = NGC_DTK;
        else if (0==strcmp(val,"PCM16BE"))      txth->codec = PCM16BE;
        else if (0==strcmp(val,"PCM16LE"))      txth->codec = PCM16LE;
        else if (0==strcmp(val,"PCM8"))         txth->codec = PCM8;
        else if (0==strcmp(val,"SDX2"))         txth->codec = SDX2;
        else if (0==strcmp(val,"DVI_IMA"))      txth->codec = DVI_IMA;
        else if (0==strcmp(val,"MPEG"))         txth->codec = MPEG;
        else if (0==strcmp(val,"IMA"))          txth->codec = IMA;
        else if (0==strcmp(val,"AICA"))         txth->codec = AICA;
        else if (0==strcmp(val,"MSADPCM"))      txth->codec = MSADPCM;
        else if (0==strcmp(val,"NGC_DSP"))      txth->codec = NGC_DSP;
        else if (0==strcmp(val,"DSP"))          txth->codec = NGC_DSP;
        else if (0==strcmp(val,"PCM8_U_int"))   txth->codec = PCM8_U_int;
        else if (0==strcmp(val,"PSX_bf"))       txth->codec = PSX_bf;
        else if (0==strcmp(val,"MS_IMA"))       txth->codec = MS_IMA;
        else if (0==strcmp(val,"PCM8_U"))       txth->codec = PCM8_U;
        else if (0==strcmp(val,"APPLE_IMA4"))   txth->codec = APPLE_IMA4;
        else if (0==strcmp(val,"ATRAC3"))       txth->codec = ATRAC3;
        else if (0==strcmp(val,"ATRAC3PLUS"))   txth->codec = ATRAC3PLUS;
        else if (0==strcmp(val,"XMA1"))         txth->codec = XMA1;
        else if (0==strcmp(val,"XMA2"))         txth->codec = XMA2;
        else if (0==strcmp(val,"FFMPEG"))       txth->codec = FFMPEG;
        else if (0==strcmp(val,"AC3"))          txth->codec = AC3;
        else if (0==strcmp(val,"PCFX"))         txth->codec = PCFX;
        else if (0==strcmp(val,"PCM4"))         txth->codec = PCM4;
        else if (0==strcmp(val,"PCM4_U"))       txth->codec = PCM4_U;
        else if (0==strcmp(val,"OKI16"))        txth->codec = OKI16;
        else goto fail;
    }
    else if (0==strcmp(key,"codec_mode")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->codec_mode)) goto fail;
    }
    else if (0==strcmp(key,"value_mul") || 0==strcmp(key,"value_*")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->value_mul)) goto fail;
    }
    else if (0==strcmp(key,"value_div") || 0==strcmp(key,"value_/")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->value_div)) goto fail;
    }
    else if (0==strcmp(key,"value_add") || 0==strcmp(key,"value_+")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->value_add)) goto fail;
    }
    else if (0==strcmp(key,"value_sub") || 0==strcmp(key,"value_-")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->value_sub)) goto fail;
    }
    else if (0==strcmp(key,"id_value")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->id_value)) goto fail;
    }
    else if (0==strcmp(key,"id_offset")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->id_offset)) goto fail;
        if (txth->id_value != txth->id_offset) /* evaluate current ID */
            goto fail;
    }
    else if (0==strcmp(key,"interleave")) {
        if (0==strcmp(val,"half_size")) {
            if (txth->channels == 0) goto fail;
            txth->interleave = txth->data_size / txth->channels;
        }
        else {
            if (!parse_num(txth->streamHead,txth,val, &txth->interleave)) goto fail;
        }
    }
    else if (0==strcmp(key,"interleave_last")) {
        if (0==strcmp(val,"auto")) {
            if (txth->channels > 0 && txth->interleave > 0)
                txth->interleave_last = (txth->data_size % (txth->interleave * txth->channels)) / txth->channels;
        }
        else {
            if (!parse_num(txth->streamHead,txth,val, &txth->interleave_last)) goto fail;
        }
    }
    else if (0==strcmp(key,"channels")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->channels)) goto fail;
    }
    else if (0==strcmp(key,"sample_rate")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->sample_rate)) goto fail;
    }
    else if (0==strcmp(key,"start_offset")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->start_offset)) goto fail;
        if (!txth->data_size_set) {
            txth->data_size = !txth->streamBody ? 0 :
                    get_streamfile_size(txth->streamBody) - txth->start_offset; /* re-evaluate */
        }
    }
    else if (0==strcmp(key,"data_size")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->data_size)) goto fail;
        txth->data_size_set = 1;
    }
    else if (0==strcmp(key,"sample_type")) {
        if (0==strcmp(val,"samples")) txth->sample_type = 0;
        else if (0==strcmp(val,"bytes")) txth->sample_type = 1;
        else if (0==strcmp(val,"blocks")) txth->sample_type = 2;
        else goto fail;
    }
    else if (0==strcmp(key,"num_samples")) {
        if (0==strcmp(val,"data_size")) {
            txth->num_samples = get_bytes_to_samples(txth, txth->data_size);
            txth->num_samples_data_size = 1;
        }
        else {
            if (!parse_num(txth->streamHead,txth,val, &txth->num_samples)) goto fail;
            if (txth->sample_type==1)
                txth->num_samples = get_bytes_to_samples(txth, txth->num_samples);
            if (txth->sample_type==2)
                txth->num_samples = get_bytes_to_samples(txth, txth->num_samples * (txth->interleave*txth->channels));
        }
    }
    else if (0==strcmp(key,"loop_start_sample")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->loop_start_sample)) goto fail;
        if (txth->sample_type==1)
            txth->loop_start_sample = get_bytes_to_samples(txth, txth->loop_start_sample);
        if (txth->sample_type==2)
            txth->loop_start_sample = get_bytes_to_samples(txth, txth->loop_start_sample * (txth->interleave*txth->channels));
        if (txth->loop_adjust)
            txth->loop_start_sample += txth->loop_adjust;
    }
    else if (0==strcmp(key,"loop_end_sample")) {
        if (0==strcmp(val,"data_size")) {
            txth->loop_end_sample = get_bytes_to_samples(txth, txth->data_size);
        }
        else {
            if (!parse_num(txth->streamHead,txth,val, &txth->loop_end_sample)) goto fail;
            if (txth->sample_type==1)
                txth->loop_end_sample = get_bytes_to_samples(txth, txth->loop_end_sample);
            if (txth->sample_type==2)
                txth->loop_end_sample = get_bytes_to_samples(txth, txth->loop_end_sample * (txth->interleave*txth->channels));
        }
        if (txth->loop_adjust)
            txth->loop_end_sample += txth->loop_adjust;
    }
    else if (0==strcmp(key,"skip_samples")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->skip_samples)) goto fail;
        txth->skip_samples_set = 1;
        if (txth->sample_type==1)
            txth->skip_samples = get_bytes_to_samples(txth, txth->skip_samples);
        if (txth->sample_type==2)
            txth->skip_samples = get_bytes_to_samples(txth, txth->skip_samples * (txth->interleave*txth->channels));
    }
    else if (0==strcmp(key,"loop_adjust")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->loop_adjust)) goto fail;
        if (txth->sample_type==1)
            txth->loop_adjust = get_bytes_to_samples(txth, txth->loop_adjust);
        if (txth->sample_type==2)
            txth->loop_adjust = get_bytes_to_samples(txth, txth->loop_adjust * (txth->interleave*txth->channels));
    }
    else if (0==strcmp(key,"loop_flag")) {
        if (0==strcmp(val,"auto"))  {
            txth->loop_flag_auto = 1;
        }
        else {
            if (!parse_num(txth->streamHead,txth,val, &txth->loop_flag)) goto fail;
            txth->loop_flag_set = 1;
            if (txth->loop_flag == 0xFFFF || txth->loop_flag == 0xFFFFFFFF) { /* normally -1 = no loop */
                txth->loop_flag = 0;
            }
        }
    }
    else if (0==strcmp(key,"coef_offset")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->coef_offset)) goto fail;
    }
    else if (0==strcmp(key,"coef_spacing")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->coef_spacing)) goto fail;
    }
    else if (0==strcmp(key,"coef_endianness")) {
        if (val[0]=='B' && val[1]=='E')
            txth->coef_big_endian = 1;
        else if (val[0]=='L' && val[1]=='E')
            txth->coef_big_endian = 0;
        else if (!parse_num(txth->streamHead,txth,val, &txth->coef_big_endian)) goto fail;
    }
    else if (0==strcmp(key,"coef_mode")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->coef_mode)) goto fail;
    }
    else if (0==strcmp(key,"psx_loops")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->coef_mode)) goto fail;
    }
    else if (0==strcmp(key,"subsong_count")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->subsong_count)) goto fail;
    }
    else if (0==strcmp(key,"subsong_offset")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->subsong_offset)) goto fail;
    }
    else if (0==strcmp(key,"name_offset")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->name_offset)) goto fail;
        txth->name_offset_set = 1;
        /* special subsong adjustment */
        if (txth->subsong_offset)
            txth->name_offset = txth->name_offset + txth->subsong_offset * (txth->target_subsong - 1);
    }
    else if (0==strcmp(key,"name_size")) {
        if (!parse_num(txth->streamHead,txth,val, &txth->name_size)) goto fail;
    }
    else if (0==strcmp(key,"header_file")) {
        if (txth->streamhead_opened) {
            close_streamfile(txth->streamHead);
            txth->streamHead = NULL;
            txth->streamhead_opened = 0;
        }

        if (0==strcmp(val,"null")) { /* reset */
            if (!txth->streamfile_is_txth) {
                txth->streamHead = txth->streamFile;
            }
        }
        else if (val[0]=='*' && val[1]=='.') { /* basename + extension */
            txth->streamHead = open_streamfile_by_ext(txth->streamFile, (val+2));
            if (!txth->streamHead) goto fail;
            txth->streamhead_opened = 1;
        }
        else { /* open file */
            fix_dir_separators(val); /* clean paths */

            txth->streamHead = open_streamfile_by_filename(txth->streamFile, val);
            if (!txth->streamHead) goto fail;
            txth->streamhead_opened = 1;
        }
    }
    else if (0==strcmp(key,"body_file")) {
        if (txth->streambody_opened) {
            close_streamfile(txth->streamBody);
            txth->streamBody = NULL;
            txth->streambody_opened = 0;
        }

        if (0==strcmp(val,"null")) { /* reset */
            if (!txth->streamfile_is_txth) {
                txth->streamBody = txth->streamFile;
            }
        }
        else if (val[0]=='*' && val[1]=='.') { /* basename + extension */
            txth->streamBody = open_streamfile_by_ext(txth->streamFile, (val+2));
            if (!txth->streamBody) goto fail;
            txth->streambody_opened = 1;
        }
        else { /* open file */
            fix_dir_separators(val); /* clean paths */

            txth->streamBody = open_streamfile_by_filename(txth->streamFile, val);
            if (!txth->streamBody) goto fail;
            txth->streambody_opened = 1;
        }

        /* use body as header when opening a .txth directly to simplify things */
        if (txth->streamfile_is_txth && !txth->streamhead_opened) {
            txth->streamHead = txth->streamBody;
        }

        txth->data_size = !txth->streamBody ? 0 :
                get_streamfile_size(txth->streamBody) - txth->start_offset; /* re-evaluate */
    }
    else {
        VGM_LOG("TXTH: unknown key=%s, val=%s\n", key,val);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static int parse_num(STREAMFILE * streamFile, txth_header * txth, const char * val, uint32_t * out_value) {
    /* out_value can be these, save before modifying */
    uint32_t value_mul = txth->value_mul;
    uint32_t value_div = txth->value_div;
    uint32_t value_add = txth->value_add;
    uint32_t value_sub = txth->value_sub;
    uint32_t subsong_offset = txth->subsong_offset;

    if (val[0] == '@') { /* offset */
        uint32_t offset = 0;
        char ed1 = 'L', ed2 = 'E';
        int size = 4;
        int big_endian = 0;
        int hex = (val[1]=='0' && val[2]=='x');

        /* can happen when loading .txth and not setting body/head */
        if (!streamFile)
            goto fail;

        /* read exactly N fields in the expected format */
        if (strchr(val,':') && strchr(val,'$')) {
            if (sscanf(val, hex ? "@%x:%c%c$%i" : "@%u:%c%c$%i", &offset, &ed1,&ed2, &size) != 4) goto fail;
        } else if (strchr(val,':')) {
            if (sscanf(val, hex ? "@%x:%c%c" : "@%u:%c%c", &offset, &ed1,&ed2) != 3) goto fail;
        } else if (strchr(val,'$')) {
            if (sscanf(val, hex ? "@%x$%i" : "@%u$%i", &offset, &size) != 2) goto fail;
        } else {
            if (sscanf(val, hex ? "@%x" : "@%u", &offset) != 1) goto fail;
        }

        if (/*offset < 0 ||*/ offset > get_streamfile_size(streamFile))
            goto fail;

        if (ed1 == 'B' && ed2 == 'E')
            big_endian = 1;
        else if (!(ed1 == 'L' && ed2 == 'E'))
            goto fail;

        if (subsong_offset)
            offset = offset + subsong_offset * (txth->target_subsong - 1);

        switch(size) {
            case 1: *out_value = read_8bit(offset,streamFile); break;
            case 2: *out_value = big_endian ? (uint16_t)read_16bitBE(offset,streamFile) : (uint16_t)read_16bitLE(offset,streamFile); break;
            case 3: *out_value = (big_endian ? (uint32_t)read_32bitBE(offset,streamFile) : (uint32_t)read_32bitLE(offset,streamFile)) & 0x00FFFFFF; break;
            case 4: *out_value = big_endian ? (uint32_t)read_32bitBE(offset,streamFile) : (uint32_t)read_32bitLE(offset,streamFile); break;
            default: goto fail;
        }
    }
    else if (val[0] >= '0' && val[0] <= '9') { /* unsigned constant */
        int hex = (val[0]=='0' && val[1]=='x');

        if (sscanf(val, hex ? "%x" : "%u", out_value)!=1)
            goto fail;
    }
    else { /* known field */
        if      (0==strcmp(val,"interleave"))           *out_value = txth->interleave;
        if      (0==strcmp(val,"interleave_last"))      *out_value = txth->interleave_last;
        else if (0==strcmp(val,"channels"))             *out_value = txth->channels;
        else if (0==strcmp(val,"sample_rate"))          *out_value = txth->sample_rate;
        else if (0==strcmp(val,"start_offset"))         *out_value = txth->start_offset;
        else if (0==strcmp(val,"data_size"))            *out_value = txth->data_size;
        else if (0==strcmp(val,"num_samples"))          *out_value = txth->num_samples;
        else if (0==strcmp(val,"loop_start_sample"))    *out_value = txth->loop_start_sample;
        else if (0==strcmp(val,"loop_end_sample"))      *out_value = txth->loop_end_sample;
        else if (0==strcmp(val,"subsong_count"))        *out_value = txth->subsong_count;
        else if (0==strcmp(val,"subsong_offset"))       *out_value = txth->subsong_offset;
        else goto fail;
    }

    /* operators, but only if current value wasn't set to 0 right before */
    if (value_mul && txth->value_mul)
        *out_value = (*out_value) * value_mul;
    if (value_div && txth->value_div)
        *out_value = (*out_value) / value_div;
    if (value_add && txth->value_add)
        *out_value = (*out_value) + value_add;
    if (value_sub && txth->value_sub)
        *out_value = (*out_value) - value_sub;

    //;VGM_LOG("TXTH: val=%s, read %u (0x%x)\n", val, *out_value, *out_value);
    return 1;
fail:
    return 0;
}

static int get_bytes_to_samples(txth_header * txth, uint32_t bytes) {
    if (!txth->channels)
        return 0; /* div-by-zero is no fun */

    switch(txth->codec) {
        case MS_IMA:
            if (!txth->interleave) return 0;
            return ms_ima_bytes_to_samples(bytes, txth->interleave, txth->channels);
        case XBOX:
            return xbox_ima_bytes_to_samples(bytes, txth->channels);
        case NGC_DSP:
            return dsp_bytes_to_samples(bytes, txth->channels);
        case PSX:
        case PSX_bf:
            return ps_bytes_to_samples(bytes, txth->channels);
        case PCM16BE:
        case PCM16LE:
            return pcm_bytes_to_samples(bytes, txth->channels, 16);
        case PCM8:
        case PCM8_U_int:
        case PCM8_U:
            return pcm_bytes_to_samples(bytes, txth->channels, 8);
        case PCM4:
        case PCM4_U:
            return pcm_bytes_to_samples(bytes, txth->channels, 4);
        case MSADPCM:
            if (!txth->interleave) return 0;
            return msadpcm_bytes_to_samples(bytes, txth->interleave, txth->channels);
        case ATRAC3:
            if (!txth->interleave) return 0;
            return atrac3_bytes_to_samples(bytes, txth->interleave);
        case ATRAC3PLUS:
            if (!txth->interleave) return 0;
            return atrac3plus_bytes_to_samples(bytes, txth->interleave);

        /* XMA bytes-to-samples is done at the end as the value meanings are a bit different */
        case XMA1:
        case XMA2:
            return bytes; /* preserve */

        case AC3:
            if (!txth->interleave) return 0;
            return bytes / txth->interleave * 256 * txth->channels;

        case IMA:
        case DVI_IMA:
            return ima_bytes_to_samples(bytes, txth->channels);
        case AICA:
            return aica_bytes_to_samples(bytes, txth->channels);
        case PCFX:
        case OKI16:
            return oki_bytes_to_samples(bytes, txth->channels);

        /* untested */
        case SDX2:
            return bytes;
        case NGC_DTK:
            return bytes / 0x20 * 28; /* always stereo */
        case APPLE_IMA4:
            if (!txth->interleave) return 0;
            return (bytes / txth->interleave) * (txth->interleave - 2) * 2;

        case MPEG: /* a bit complex */
        case FFMPEG: /* too complex, try after init */
        default:
            return 0;
    }
}
