#include "meta.h"
#include "../coding/coding.h"
#include "../util/m2_psb.h"


//todo prepare multichannel
#define PSB_MAX_LAYERS 1

typedef enum { PCM, RIFF_AT3, XMA2, MSADPCM, XWMA, DSP, OPUSNX, RIFF_AT9, VAG } psb_codec_t;
typedef struct {
    const char* id; /* format */
    const char* spec; /* platform */
    const char* ext; /* codec extension (not always) */
    const char* voice; /* base name (mandatory) */
    const char* file; /* original name, often but not always same as voice (optional?) */
    const char* uniq; /* unique name, typically same as file without extension (optional)  */
    const char* wav; /* same as file (optional) */
} psb_temp_t;
typedef struct {
    psb_temp_t* tmp;
    psb_codec_t codec;
    char readable_name[STREAM_NAME_SIZE];

    int total_subsongs;
    int target_subsong;

    /* chunks references */
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t intro_offset;
    uint32_t intro_size;
    uint32_t fmt_offset;
    uint32_t fmt_size;
    uint32_t dpds_offset;
    uint32_t dpds_size;

    int layers;
    int channels;
    int format;
    int sample_rate;
    int block_size;
    int avg_bitrate;
    int bps;

    int32_t num_samples;
    int32_t intro_samples;
    int32_t skip_samples;
    int loop_flag;
    int32_t loop_start;
    int32_t loop_end;

} psb_header_t;


static int parse_psb(STREAMFILE* sf, psb_header_t* psb);


/* PSB - M2 container [Sega Vintage Collection (multi), Legend of Mana (multi)] */
VGMSTREAM* init_vgmstream_psb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    psb_header_t psb = {0};


    /* checks */
    if (!is_id32be(0x00,sf, "PSB\0"))
        goto fail;
    if (!check_extensions(sf, "psb"))
        goto fail;

    if (!parse_psb(sf, &psb))
        goto fail;


    /* handle subfiles */
    {
        const char* ext = NULL;
        VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf) = NULL;

        switch(psb.codec) {
            case RIFF_AT3:   /* Sega Vintage Collection (PS3) */
                ext = "at3";
                init_vgmstream = init_vgmstream_riff;
                break;

            case VAG: /* Plastic Memories (Vita) */
                ext = "vag";
                init_vgmstream = init_vgmstream_vag;
                break;

            case RIFF_AT9: /* Plastic Memories (Vita) */
                ext = "at9";
                init_vgmstream = init_vgmstream_riff;
                break;

            default:
                break;
        }

        if (init_vgmstream != NULL) {
            STREAMFILE* temp_sf = setup_subfile_streamfile(sf, psb.stream_offset, psb.stream_size, ext);
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream(temp_sf);
            close_streamfile(temp_sf);
            if (!vgmstream) goto fail;

            vgmstream->num_streams = psb.total_subsongs;
            strncpy(vgmstream->stream_name, psb.readable_name, STREAM_NAME_SIZE);
            return vgmstream;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(psb.channels, psb.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PSB;
    vgmstream->sample_rate = psb.sample_rate;
    vgmstream->num_samples = psb.num_samples;
    vgmstream->loop_start_sample = psb.loop_start;
    vgmstream->loop_end_sample = psb.loop_end;
    vgmstream->num_streams = psb.total_subsongs;
    vgmstream->stream_size = psb.stream_size;

    switch(psb.codec) {
        case PCM:
            switch(psb.bps) {
                case 16: vgmstream->coding_type = coding_PCM16LE; break; /* Legend of Mana (PC), Namco Museum Archives Vol.1 (PC) */
                case 24: vgmstream->coding_type = coding_PCM24LE; break; /* Legend of Mana (PC) */
                default: 
                    vgm_logi("PSB: unknown bps %i (report)\n", psb.bps);
                    goto fail;
            }
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = psb.block_size / psb.channels;
            if (!vgmstream->num_samples)
                vgmstream->num_samples = pcm_bytes_to_samples(psb.stream_size, psb.channels, psb.bps);
            break;

        case MSADPCM: /* [Senxin Aleste (AC)] */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = psb.block_size;
            if (!vgmstream->num_samples)
                vgmstream->num_samples = msadpcm_bytes_to_samples(psb.stream_size, psb.block_size, psb.channels);
            break;

#ifdef VGM_USE_FFMPEG
        case XWMA: { /* [Senxin Aleste (AC)] */
            vgmstream->codec_data = init_ffmpeg_xwma(sf, psb.stream_offset, psb.stream_size, psb.format, psb.channels, psb.sample_rate, psb.avg_bitrate, psb.block_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (!vgmstream->num_samples) {
                vgmstream->num_samples = xwma_dpds_get_samples(sf, psb.dpds_offset, psb.dpds_size, psb.channels, 0);
            }

            break;
        }

        case XMA2: { /* Sega Vintage Collection (X360) */
            uint8_t buf[0x100];
            size_t bytes;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, sizeof(buf), psb.fmt_offset, psb.fmt_size, psb.stream_size, sf, 1);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf, bytes, psb.stream_offset, psb.stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, psb.stream_offset, psb.stream_size, psb.fmt_offset, 1,1);
            break;
        }
#endif

        case DSP: /* Legend of Mana (Switch) */
        case OPUSNX: /* Legend of Mana (Switch) */
        default:
            vgm_logi("PSB: not implemented (ignore)\n");
            goto fail;
    }

    strncpy(vgmstream->stream_name, psb.readable_name, STREAM_NAME_SIZE);

    if (!vgmstream_open_stream(vgmstream, sf, psb.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/*****************************************************************************/

static int prepare_fmt(STREAMFILE* sf, psb_header_t* psb) {
    uint32_t offset = psb->fmt_offset;
    if (!offset)
        return 1; /* other codec, probably */

    psb->format         = read_u16le(offset + 0x00,sf);
    if (psb->format == 0x6601) { /* X360 */
        psb->format         = read_u16be(offset + 0x00,sf);
        psb->channels       = read_u16be(offset + 0x02,sf);
        psb->sample_rate    = read_u32be(offset + 0x04,sf);
        xma2_parse_fmt_chunk_extra(sf, 
            offset, 
            &psb->loop_flag, 
            &psb->num_samples,
            &psb->loop_start,
            &psb->loop_end,
            1);
    }
    else {
        psb->channels       = read_u16le(offset + 0x02,sf);
        psb->sample_rate    = read_u32le(offset + 0x04,sf);
        psb->avg_bitrate    = read_u32le(offset + 0x08,sf);
        psb->block_size     = read_u16le(offset + 0x0c,sf);
        psb->bps            = read_u16le(offset + 0x0e,sf);
        /* 0x10+ varies */
        
        switch(psb->format) {
            case 0x0002:
                if (!msadpcm_check_coefs(sf, offset + 0x14))
                    goto fail;
                break;
            default:
                break;
        }
        
    }

    return 1;
fail:
    return 0;
}

static int prepare_codec(STREAMFILE* sf, psb_header_t* psb) {
    const char* spec = psb->tmp->spec;
    const char* ext = psb->tmp->ext;

    /* try fmt (most common) */
    if (psb->format != 0) {
        switch(psb->format) {
            case 0x01:
                psb->codec = PCM;
                break;
            case 0x02:
                psb->codec = MSADPCM;
                break;
            case 0x161:
                psb->codec = XWMA;
                break;
            case 0x166:
                psb->codec = XMA2;
                break;
            default:
                goto fail;
        }
        return 1;
    }

    /* try console strings */
    if (!spec)
        goto fail;
    
    if (strcmp(spec, "nx") == 0) {
        if (!ext)
            goto fail;
        
        if (strcmp(ext, ".opus") == 0) {
            psb->codec = OPUSNX;
            return 1;
        }

        if (strcmp(ext, ".adpcm") == 0) {
            psb->codec = DSP;
            return 1;
        }
    }

    if (strcmp(spec, "ps3") == 0) {
        psb->codec = RIFF_AT3;
        return 1;
    }

    if (strcmp(spec, "vita") == 0) {
        if (is_id32be(psb->stream_offset, sf, "RIFF"))
            psb->codec = RIFF_AT9;
        else
            psb->codec = VAG;
        return 1;
    }

fail:
    vgm_logi("PSB: unknown codec (report)\n");
    return 0;
}


static int prepare_name(psb_header_t* psb) {
    char* buf = psb->readable_name;
    int buf_size = sizeof(psb->readable_name);
    const char* main_name = psb->tmp->voice;
    const char* sub_name = psb->tmp->uniq;
    int main_len;

    if (!sub_name)
        sub_name = psb->tmp->wav;
    if (!sub_name)
        sub_name = psb->tmp->file;

    if (!main_name) /* shouldn't happen */
        return 1;

    /* sometimes we have main="bgm01", sub="bgm01.wav" = detect and ignore */
    main_len = strlen(main_name);
    if (sub_name && strncmp(main_name, sub_name, main_len) == 0) {
        if (sub_name[main_len] == '\0' || strcmp(sub_name + main_len, ".wav") == 0)
            sub_name = NULL;
    }

    if (sub_name) {
        snprintf(buf, buf_size, "%s/%s", main_name, sub_name);
    }
    else {
        snprintf(buf, buf_size, "%s", main_name);
    }

    return 1;
}

static int prepare_psb_extra(STREAMFILE* sf, psb_header_t* psb) {
    if (!prepare_fmt(sf, psb))
        goto fail;
    if (!prepare_codec(sf, psb))
        goto fail;
    if (!prepare_name(psb))
        goto fail;
    return 1;
fail:
    return 0;    
}


/* channelList is an array (N layers, though typically only mono codecs like DSP) of objects:
 * - archData: resource offset (RIFF) or sub-object
 *   - data/fmt/loop/wav
 *   - data/ext/samprate
 *   - body/channelCount/ext/intro/loop/samprate [Legend of Mana (Switch)]
 *     - body: data/sampleCount/skipSampleCount, intro: data/sampleCount
 *   - data/dpds/fmt/wav/loop
 * - pan: array [N.0 .. 0.N] (when N layers, in practice just a wonky L/R definition) 
 */
static int parse_psb_channels(psb_header_t* psb, psb_node_t* nchans) {
    int i;
    psb_node_t nchan, narch, nsub, node;

    psb->layers = psb_node_get_count(nchans);
    if (psb->layers == 0) goto fail;
    if (psb->layers > PSB_MAX_LAYERS) goto fail;

    for (i = 0; i < psb->layers; i++) {
        psb_data_t data;
        psb_type_t type;

        psb_node_by_index(nchans, i, &nchan);

        /* try to get possible keys (without overwritting), results will be handled and validated later as combos get complex */
        psb_node_by_key(&nchan, "archData", &narch);
        type = psb_node_get_type(&narch);
        switch (type) {
            case PSB_TYPE_DATA: /* Sega Vintage Collection (PS3) */
                data = psb_node_get_result(&narch).data;
                psb->stream_offset = data.offset;
                psb->stream_size = data.size;
                break;

            case PSB_TYPE_OBJECT: /* rest */
                /* typically:
                 * - data + fmt + others
                 * - body {data + fmt} + intro {data + fmt} + others [Legend of Mana (Switch)]
                 */

                data = psb_node_get_data(&narch, "data");
                if (data.offset) {
                    psb->stream_offset = data.offset;
                    psb->stream_size = data.size;
                }

                data = psb_node_get_data(&narch, "fmt");
                if (data.offset) {
                    psb->fmt_offset = data.offset;
                    psb->fmt_size = data.size;
                }

                if (psb_node_by_key(&narch, "loop", &node)) {
                    /* can be found as "false" with body+intro */
                    if (psb_node_get_type(&node) == PSB_TYPE_ARRAY) {
                        //todo improve
                        psb_node_by_index(&node, 0, &nsub);
                        psb->loop_start = psb_node_get_result(&nsub).num;

                        psb_node_by_index(&node, 1, &nsub);
                        psb->loop_end = psb_node_get_result(&nsub).num + psb->loop_start; /* duration */
                    }
                }

#if 0
                if (psb_node_by_key(&narch, "body", &node)) {
                    data = psb_node_get_data(&node, "data");
                    psb->stream_offset = data.offset;
                    psb->stream_size = data.size;
                    psb->num_samples = psb_node_get_integer(&node, "sampleCount");
                    psb->skip_samples = psb_node_get_integer(&node, "skipSampleCount");
                }

                if (psb_node_by_key(&narch, "intro", &node)) {
                    data = psb_node_get_data(&node, "data");
                    psb->stream_offset = data.offset;
                    psb->stream_size = data.size;
                    psb->num_samples = psb_node_get_integer(&node, "sampleCount");
                    psb->skip_samples = psb_node_get_integer(&node, "skipSampleCount");
                }
#endif

                data = psb_node_get_data(&narch, "dpds");
                if (data.offset) {
                    psb->dpds_offset = data.offset;
                    psb->dpds_size = data.size;
                }

                psb->sample_rate = (int)psb_node_get_float(&narch, "samprate");

                psb->tmp->wav = psb_node_get_string(&narch, "wav");

                /* background: false? 
                 */
                break;

            default:
                goto fail;
        }
    }
    return 1;
fail:
    VGM_LOG("psb: can't parse channel\n");
    return 0;
}


/* parse a single archive, that can contain extra info here or inside channels */
static int parse_psb_voice(psb_header_t* psb, psb_node_t* nvoice) {
    psb_node_t nsong, nchans;


    psb->total_subsongs = psb_node_get_count(nvoice);
    if (psb->target_subsong == 0) psb->target_subsong = 1;
    if (psb->total_subsongs <= 0 || psb->target_subsong > psb->total_subsongs) goto fail;


    /* target voice and stream info */
    if (!psb_node_by_index(nvoice, psb->target_subsong - 1, &nsong))
        goto fail;
    psb->tmp->voice = psb_node_get_key(nvoice, psb->target_subsong - 1);

    psb_node_by_key(&nsong, "channelList", &nchans);
    if (!parse_psb_channels(psb, &nchans))
        goto fail;


    /* unsure of meaning but must exist (usually 0/1) */
    if (psb_node_exists(&nsong, "device") <= 0)
        goto fail;

    /* names (optional) */
    psb->tmp->file = psb_node_get_string(&nsong, "file");
    psb->tmp->uniq = psb_node_get_string(&nsong, "uniq");

    /* optional loop flag (loop points go in channels, or implicit in fmt/RIFF) */
    if (!psb->loop_flag) {
        psb->loop_flag = psb_node_get_integer(&nsong, "loop") > 1;
        /* There is also loopstr/loopStr = "all" when "loop"=2 and "none" when "loop"=0
         * SFX set loop=0, and sometimes songs that look like they could do full loops do too */
    }

    /* other optional keys:
     * - quality: ? (1=MSADPCM, 2=OPUSNX/PCM)
     * - priority: f32, -1.0, 1.0 or 10.0 = max?
     * - type: 0/1? (internal classification?)
     * - volume: 0.0 .. 1.0
     * - group?
     */

    return 1;
fail:
    VGM_LOG("psb: can't parse voice\n");
    return 0;
}

/* .psb is binary JSON-like structure that can be used to hold various formats, we want audio data:
 * - (root): (object)
 *   - "id": (format string)
 *   - "spec": (platform string)
 *   - "version": (float)
 *   - "voice": (objects, one per subsong)
 *     - (voice name 1): (object)
 *       - "channelList": (array of N objects)
 *          - "archData": (main audio part, varies per game/platform/codec)
 *          - "device": ?
 *     ...
 *     - (voice name N): ... 
 * From decompilations, audio code reads common keys up to "archData", then depends on game (not unified).
 * Keys are (seemingly) stored in text order.
 */
static int parse_psb(STREAMFILE* sf, psb_header_t* psb) {
    psb_temp_t tmp;
    psb_context_t* ctx = NULL;
    psb_node_t nroot, nvoice;
    float version;

    psb->tmp = &tmp;
    psb->target_subsong = sf->stream_index;

    ctx = psb_init(sf);
    if (!ctx) goto fail;
    //psb_print(ctx);

    /* main process */
    psb_get_root(ctx, &nroot);

    /* format definition, non-audio IDs include "motion", "font", or no "id" at all */
    psb->tmp->id = psb_node_get_string(&nroot, "id");
    if (!psb->tmp->id || strcmp(psb->tmp->id, "sound_archive") != 0) {
        /* "sound" is just a list of available "sound_archive" */
        if (psb->tmp->id && strcmp(psb->tmp->id, "sound") == 0)
            vgm_logi("PSB: empty archive type '%s' (ignore)\n", psb->tmp->id);
        else
            vgm_logi("PSB: unsupported archive type '%s' (ignore?)\n", psb->tmp->id);
        goto fail;
    }

    /* platform: x360/ps3/win/nx/etc */
    psb->tmp->spec = psb_node_get_string(&nroot, "spec");

    /* enforced by M2 code */
    version = psb_node_get_float(&nroot, "version");
    if (version < 1.02f || version > 1.02f) {
        vgm_logi("PSB: unsupported version %f (report)\n", version);
        goto fail;
    }

    /* main subsong */
    psb_node_by_key(&nroot, "voice", &nvoice);
    if (!parse_psb_voice(psb, &nvoice))
        goto fail;

    /* post stuff before closing PSB */
    if (!prepare_psb_extra(sf, psb))
        goto fail;

    psb->tmp = NULL;
    psb_close(ctx);
    return 1;
fail:
    psb_close(ctx);
    VGM_LOG("psb: can't parse PSB\n");
    return 0;
}

#if 0
typedef struct {
    void* init;
    const char* id32;
    const char* exts;
} metadef_t;

metadef_t md_psb = {
    .init = init_vgmstream_psb,
    .exts = "psb",
    .id32 = "PSB\0", //24b/masked IDs?
    .id32 = get_id32be("PSB\0"), //???
    .idfn = psb_check_id,
}
#endif
