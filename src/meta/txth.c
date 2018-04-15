#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

#define TXT_LINE_MAX 0x2000

/* known TXTH types */
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
    AC3 = 23,         /* AC3/SPDIF */
} txth_type;

typedef struct {
    txth_type codec;
    uint32_t codec_mode;
    uint32_t interleave;

    uint32_t id_value;
    uint32_t id_offset;

    uint32_t channels;
    uint32_t sample_rate;

    uint32_t data_size;
    int data_size_set;
    uint32_t start_offset;

    int sample_type_bytes;
    uint32_t num_samples;
    uint32_t loop_start_sample;
    uint32_t loop_end_sample;
    uint32_t loop_adjust;
    int skip_samples_set;
    uint32_t skip_samples;

    uint32_t loop_flag;
    int loop_flag_set;

    uint32_t coef_offset;
    uint32_t coef_spacing;
    uint32_t coef_big_endian;
    uint32_t coef_mode;

} txth_header;

static STREAMFILE * open_txth(STREAMFILE * streamFile);
static int parse_txth(STREAMFILE * streamFile, STREAMFILE * streamText, txth_header * txth);
static int parse_keyval(STREAMFILE * streamFile, STREAMFILE * streamText, txth_header * txth, const char * key, const char * val);
static int parse_num(STREAMFILE * streamFile, const char * val, uint32_t * out_value);
static int get_bytes_to_samples(txth_header * txth, uint32_t bytes);


/* TXTH - an artificial "generic" header for headerless streams.
 * Similar to GENH, but with a single separate .txth file in the dir and text-based. */
VGMSTREAM * init_vgmstream_txth(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamText = NULL;
    txth_header txth = {0};
    coding_t coding;
    int i, j;


    /* reject .txth as the CLI can open and decode with itself */
    if (check_extensions(streamFile, "txth"))
        goto fail;

    /* no need for ID or ext checks -- if a .TXTH exists all is good
     * (player still needs to accept the streamfile's ext, so at worst rename to .vgmstream) */
    streamText = open_txth(streamFile);
    if (!streamText) goto fail;

    /* process the text file */
    if (!parse_txth(streamFile, streamText, &txth))
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
        default:
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(txth.channels,txth.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = txth.sample_rate;
    vgmstream->num_samples = txth.num_samples;
    vgmstream->loop_start_sample = txth.loop_start_sample;
    vgmstream->loop_end_sample = txth.loop_end_sample;

    /* codec specific (taken from GENH with minimal changes) */
    switch (coding) {
        case coding_PCM8_U_int:
            vgmstream->layout_type = layout_none;
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
            vgmstream->interleave_block_size = txth.interleave;
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
            if (txth.codec_mode == 1) {
                if (!txth.interleave) goto fail; /* creates garbage */
                coding = coding_XBOX_IMA_int;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = txth.interleave;
            }
            else {
                vgmstream->layout_type = layout_none;
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
                if (txth.coef_mode == 0) {
                    for (j=0;j<16;j++) {
                        vgmstream->ch[i].adpcm_coef[j] = read_16bit(txth.coef_offset + i*txth.coef_spacing  + j*2,streamFile);
                    }
                }
                else {
                    goto fail; //IDK what is this
                    /*
                    for (j=0;j<8;j++) {
                        vgmstream->ch[i].adpcm_coef[j*2]=read_16bit(coef[i]+j*2,streamFile);
                        vgmstream->ch[i].adpcm_coef[j*2+1]=read_16bit(coef_splitted[i]+j*2,streamFile);
                    }
                    */
                }
            }

            break;
#ifdef VGM_USE_MPEG
        case coding_MPEG_layer3:
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_mpeg(streamFile, txth.start_offset, &coding, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *ffmpeg_data = NULL;

            if (txth.codec == FFMPEG || txth.codec == AC3) {
                /* default FFmpeg */
                ffmpeg_data = init_ffmpeg_offset(streamFile, txth.start_offset,txth.data_size);
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
                    int block_size = txth.interleave ? txth.interleave : 2048;
                    int block_count = txth.data_size / block_size;

                    bytes = ffmpeg_make_riff_xma2(buf, 200, vgmstream->num_samples, txth.data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
                }
                else {
                    goto fail;
                }
                if (bytes <= 0) goto fail;

                ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, txth.start_offset,txth.data_size);
                if ( !ffmpeg_data ) goto fail;
            }

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->layout_type = layout_none;

            /* force encoder delay */
            if (txth.skip_samples_set) {
                ffmpeg_set_skip_samples(ffmpeg_data, txth.skip_samples);
            }

            break;
        }
#endif
        default:
            break;
    }

#ifdef VGM_USE_FFMPEG
    if (txth.sample_type_bytes && (txth.codec == XMA1 || txth.codec == XMA2)) {
        /* manually find sample offsets */
        ms_sample_data msd;
        memset(&msd,0,sizeof(ms_sample_data));

        msd.xma_version = 1;
        msd.channels = txth.channels;
        msd.data_offset = txth.start_offset;
        msd.data_size = txth.data_size;
        msd.loop_flag = txth.loop_flag;
        msd.loop_start_b = txth.loop_start_sample;
        msd.loop_end_b   = txth.loop_end_sample;
        msd.loop_start_subframe = txth.loop_adjust & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
        msd.loop_end_subframe   = txth.loop_adjust >> 4;  /* upper 4b: subframe where the loop ends, 0..3 */

        xma_get_samples(&msd, streamFile);
        vgmstream->num_samples = msd.num_samples;
        vgmstream->loop_start_sample = msd.loop_start_sample;
        vgmstream->loop_end_sample = msd.loop_end_sample;
        //skip_samples = msd.skip_samples; //todo add skip samples
    }
#endif

    vgmstream->coding_type = coding;
    vgmstream->meta_type = meta_TXTH;


    if ( !vgmstream_open_stream(vgmstream,streamFile,txth.start_offset) )
        goto fail;

    if (streamText) close_streamfile(streamText);
    return vgmstream;

fail:
    if (streamText) close_streamfile(streamText);
    close_vgmstream(vgmstream);
    return NULL;
}


static STREAMFILE * open_txth(STREAMFILE * streamFile) {
    char filename[PATH_LIMIT];
    char fileext[PATH_LIMIT];
    STREAMFILE * streamText;

    /* try "(path/)(name.ext).txth" */
    get_streamfile_name(streamFile,filename,PATH_LIMIT);
    strcat(filename, ".txth");
    streamText = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (streamText) return streamText;

    /* try "(path/)(.ext).txth" */
    get_streamfile_path(streamFile,filename,PATH_LIMIT);
    get_streamfile_ext(streamFile,fileext,PATH_LIMIT);
    strcat(filename,".");
    strcat(filename, fileext);
    strcat(filename, ".txth");
    streamText = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (streamText) return streamText;

    /* try "(path/).txth" */
    get_streamfile_path(streamFile,filename,PATH_LIMIT);
    strcat(filename, ".txth");
    streamText = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (streamText) return streamText;

    /* not found */
    return NULL;
}

/* Simple text parser of "key = value" lines.
 * The code is meh and error handling not exactly the best. */
static int parse_txth(STREAMFILE * streamFile, STREAMFILE * streamText, txth_header * txth) {
    off_t txt_offset = 0x00;
    off_t file_size = get_streamfile_size(streamText);

    txth->data_size = get_streamfile_size(streamFile); /* for later use */

    /* skip BOM if needed */
    if (read_16bitLE(0x00, streamText) == 0xFFFE || read_16bitLE(0x00, streamText) == 0xFEFF)
        txt_offset = 0x02;

    /* read lines */
    while (txt_offset < file_size) {
        char line[TXT_LINE_MAX] = {0};
        char key[TXT_LINE_MAX] = {0}, val[TXT_LINE_MAX] = {0}; /* at least as big as a line to avoid overflows (I hope) */
        int ok, bytes_read, line_done;

        bytes_read = get_streamfile_text_line(TXT_LINE_MAX,line, txt_offset,streamText, &line_done);
        if (!line_done) goto fail;

        txt_offset += bytes_read;
        
        /* get key/val (ignores lead/trail spaces, stops at space/comment/separator) */
        ok = sscanf(line, " %[^ \t#=] = %[^ \t#\r\n] ", key,val);
        if (ok != 2) /* ignore line if no key=val (comment or garbage) */
            continue;

        if (!parse_keyval(streamFile, streamText, txth, key, val)) /* read key/val */
            goto fail;
    }

    if (!txth->loop_flag_set)
        txth->loop_flag = txth->loop_end_sample && txth->loop_end_sample != 0xFFFFFFFF;

    return 1;
fail:
    return 0;
}

static int parse_keyval(STREAMFILE * streamFile, STREAMFILE * streamText, txth_header * txth, const char * key, const char * val) {

    if (0==strcmp(key,"codec")) {
        if      (0==strcmp(val,"PSX")) txth->codec = PSX;
        else if (0==strcmp(val,"XBOX")) txth->codec = XBOX;
        else if (0==strcmp(val,"NGC_DTK")) txth->codec = NGC_DTK;
        else if (0==strcmp(val,"PCM16BE")) txth->codec = PCM16BE;
        else if (0==strcmp(val,"PCM16LE")) txth->codec = PCM16LE;
        else if (0==strcmp(val,"PCM8")) txth->codec = PCM8;
        else if (0==strcmp(val,"SDX2")) txth->codec = SDX2;
        else if (0==strcmp(val,"DVI_IMA")) txth->codec = DVI_IMA;
        else if (0==strcmp(val,"MPEG")) txth->codec = MPEG;
        else if (0==strcmp(val,"IMA")) txth->codec = IMA;
        else if (0==strcmp(val,"AICA")) txth->codec = AICA;
        else if (0==strcmp(val,"MSADPCM")) txth->codec = MSADPCM;
        else if (0==strcmp(val,"NGC_DSP")) txth->codec = NGC_DSP;
        else if (0==strcmp(val,"PCM8_U_int")) txth->codec = PCM8_U_int;
        else if (0==strcmp(val,"PSX_bf")) txth->codec = PSX_bf;
        else if (0==strcmp(val,"MS_IMA")) txth->codec = MS_IMA;
        else if (0==strcmp(val,"PCM8_U")) txth->codec = PCM8_U;
        else if (0==strcmp(val,"APPLE_IMA4")) txth->codec = APPLE_IMA4;
        else if (0==strcmp(val,"ATRAC3")) txth->codec = ATRAC3;
        else if (0==strcmp(val,"ATRAC3PLUS")) txth->codec = ATRAC3PLUS;
        else if (0==strcmp(val,"XMA1")) txth->codec = XMA1;
        else if (0==strcmp(val,"XMA2")) txth->codec = XMA2;
        else if (0==strcmp(val,"FFMPEG")) txth->codec = FFMPEG;
        else if (0==strcmp(val,"AC3")) txth->codec = AC3;
        else goto fail;
    }
    else if (0==strcmp(key,"codec_mode")) {
        if (!parse_num(streamFile,val, &txth->codec_mode)) goto fail;
    }
    else if (0==strcmp(key,"interleave")) {
        if (0==strcmp(val,"half_size")) {
            txth->interleave = txth->data_size / txth->channels;
        }
        else {
            if (!parse_num(streamFile,val, &txth->interleave)) goto fail;
        }
    }
    else if (0==strcmp(key,"id_value")) {
        if (!parse_num(streamFile,val, &txth->id_value)) goto fail;
    }
    else if (0==strcmp(key,"id_offset")) {
        if (!parse_num(streamFile,val, &txth->id_offset)) goto fail;
        if (txth->id_value != txth->id_offset) /* evaluate current ID */
            goto fail;
    }
    else if (0==strcmp(key,"channels")) {
        if (!parse_num(streamFile,val, &txth->channels)) goto fail;
    }
    else if (0==strcmp(key,"sample_rate")) {
        if (!parse_num(streamFile,val, &txth->sample_rate)) goto fail;
    }
    else if (0==strcmp(key,"start_offset")) {
        if (!parse_num(streamFile,val, &txth->start_offset)) goto fail;
        if (!txth->data_size_set)
            txth->data_size = get_streamfile_size(streamFile) - txth->start_offset; /* re-evaluate */
    }
    else if (0==strcmp(key,"data_size")) {
        if (!parse_num(streamFile,val, &txth->data_size)) goto fail;
        txth->data_size_set = 1;
    }
    else if (0==strcmp(key,"sample_type")) {
        if (0==strcmp(val,"bytes")) txth->sample_type_bytes = 1;
        else if (0==strcmp(val,"samples")) txth->sample_type_bytes = 0;
        else goto fail;
    }
    else if (0==strcmp(key,"num_samples")) {
        if (0==strcmp(val,"data_size")) {
            txth->num_samples = get_bytes_to_samples(txth, txth->data_size);
        }
        else {
            if (!parse_num(streamFile,val, &txth->num_samples)) goto fail;
            if (txth->sample_type_bytes)
                txth->num_samples = get_bytes_to_samples(txth, txth->num_samples);
        }
    }
    else if (0==strcmp(key,"loop_start_sample")) {
        if (!parse_num(streamFile,val, &txth->loop_start_sample)) goto fail;
        if (txth->sample_type_bytes)
            txth->loop_start_sample = get_bytes_to_samples(txth, txth->loop_start_sample);
        if (txth->loop_adjust)
            txth->loop_start_sample += txth->loop_adjust;
    }
    else if (0==strcmp(key,"loop_end_sample")) {
        if (0==strcmp(val,"data_size")) {
            txth->loop_end_sample = get_bytes_to_samples(txth, txth->data_size);
        }
        else {
            if (!parse_num(streamFile,val, &txth->loop_end_sample)) goto fail;
            if (txth->sample_type_bytes)
                txth->loop_end_sample = get_bytes_to_samples(txth, txth->loop_end_sample);
        }
        if (txth->loop_adjust)
            txth->loop_end_sample += txth->loop_adjust;
    }
    else if (0==strcmp(key,"skip_samples")) {
        if (!parse_num(streamFile,val, &txth->skip_samples)) goto fail;
        txth->skip_samples_set = 1;
        if (txth->sample_type_bytes)
            txth->skip_samples = get_bytes_to_samples(txth, txth->skip_samples);
    }
    else if (0==strcmp(key,"loop_adjust")) {
        if (!parse_num(streamFile,val, &txth->loop_adjust)) goto fail;
        if (txth->sample_type_bytes)
            txth->loop_adjust = get_bytes_to_samples(txth, txth->loop_adjust);
    }
    else if (0==strcmp(key,"loop_flag")) {
        if (!parse_num(streamFile,val, &txth->loop_flag)) goto fail;
        txth->loop_flag_set = 1;
    }
    else if (0==strcmp(key,"coef_offset")) {
        if (!parse_num(streamFile,val, &txth->coef_offset)) goto fail;
    }
    else if (0==strcmp(key,"coef_spacing")) {
        if (!parse_num(streamFile,val, &txth->coef_spacing)) goto fail;
    }
    else if (0==strcmp(key,"coef_endianness")) {
        if (val[0]=='B' && val[1]=='E')
            txth->coef_big_endian = 1;
        else if (val[0]=='L' && val[1]=='E')
            txth->coef_big_endian = 0;
        else if (!parse_num(streamFile,val, &txth->coef_big_endian)) goto fail;
    }
    else if (0==strcmp(key,"coef_mode")) {
        if (!parse_num(streamFile,val, &txth->coef_mode)) goto fail;
    }
    else {
        VGM_LOG("TXTH: unknown key=%s, val=%s\n", key,val);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static int parse_num(STREAMFILE * streamFile, const char * val, uint32_t * out_value) {

    if (val[0] == '@') { /* offset */
        uint32_t off = 0;
        char ed1 = 'L', ed2 = 'E';
        int size = 4;
        int big_endian = 0;
        int hex = (val[1]=='0' && val[2]=='x');

        /* read exactly N fields in the expected format */
        if (strchr(val,':') && strchr(val,'$')) {
            if (sscanf(val, hex ? "@%x:%c%c$%i" : "@%u:%c%c$%i", &off, &ed1,&ed2, &size) != 4) goto fail;
        } else if (strchr(val,':')) {
            if (sscanf(val, hex ? "@%x:%c%c" : "@%u:%c%c", &off, &ed1,&ed2) != 3) goto fail;
        } else if (strchr(val,'$')) {
            if (sscanf(val, hex ? "@%x$%i" : "@%u$%i", &off, &size) != 2) goto fail;
        } else {
            if (sscanf(val, hex ? "@%x" : "@%u", &off) != 1) goto fail;
        }

        if (off < 0 || off > get_streamfile_size(streamFile))
            goto fail;

        if (ed1 == 'B' && ed2 == 'E')
            big_endian = 1;
        else if (!(ed1 == 'L' && ed2 == 'E'))
            goto fail;

        switch(size) {
            case 1: *out_value = read_8bit(off,streamFile); break;
            case 2: *out_value = big_endian ? (uint16_t)read_16bitBE(off,streamFile) : (uint16_t)read_16bitLE(off,streamFile); break;
            case 3: *out_value = (big_endian ? (uint32_t)read_32bitBE(off,streamFile) : (uint32_t)read_32bitLE(off,streamFile)) & 0x00FFFFFF; break;
            case 4: *out_value = big_endian ? (uint32_t)read_32bitBE(off,streamFile) : (uint32_t)read_32bitLE(off,streamFile); break;
            default: goto fail;
        }
    }
    else { /* constant */
        int hex = (val[0]=='0' && val[1]=='x');

        if (sscanf(val, hex ? "%x" : "%u", out_value)!=1) goto fail;
    }

    //VGM_LOG("TXTH: value=%s, read %u\n", val, *out_value);
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

        /* untested */
        case IMA:
        case DVI_IMA:
        case SDX2:
            return bytes;
        case AICA:
            return bytes * 2 / txth->channels;
        case NGC_DTK:
            return bytes / 32 * 28; /* always stereo? */
        case APPLE_IMA4:
            if (!txth->interleave) return 0;
            return (bytes / txth->interleave) * (txth->interleave - 2) * 2;

        case MPEG: /* a bit complex */
        case FFMPEG: /* too complex, try after init */
        default:
            return 0;
    }
}
