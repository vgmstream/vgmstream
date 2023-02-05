#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

typedef enum { NONE, MSADPCM, DSP, GCADPCM, ATRAC9, RIFF_ATRAC9, KOVS, KTSS, } ktsr_codec;

#define MAX_CHANNELS 8

typedef struct {
    int total_subsongs;
    int target_subsong;
    ktsr_codec codec;

    int platform;
    int format;

    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int loop_flag;
    uint32_t extra_offset;
    uint32_t channel_layout;

    int is_external;
    uint32_t stream_offsets[MAX_CHANNELS];
    uint32_t stream_sizes[MAX_CHANNELS];

    uint32_t sound_name_offset;
    uint32_t config_name_offset;
    char name[255+1];
} ktsr_header;

static int parse_ktsr(ktsr_header* ktsr, STREAMFILE* sf);
static layered_layout_data* build_layered_atrac9(ktsr_header* ktsr, STREAMFILE *sf, uint32_t config_data);
static VGMSTREAM* init_vgmstream_ktsr_sub(STREAMFILE* sf_b, ktsr_header* ktsr, VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf), const char* ext);

/* KTSR - Koei Tecmo sound resource container */
VGMSTREAM* init_vgmstream_ktsr(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_b = NULL;
    ktsr_header ktsr = {0};
    int target_subsong = sf->stream_index;
    int separate_offsets = 0;


    /* checks */
    if (!is_id32be(0x00, sf, "KTSR"))
        goto fail;
    if (read_u32be(0x04, sf) != 0x777B481A) /* hash(?) id: 0x777B481A=as, 0x0294DDFC=st, 0xC638E69E=gc */
        goto fail;
    /* .ktsl2asbin: common [Atelier Ryza (PC/Switch), Nioh (PC)] 
     * .asbin: Warriors Orochi 4 (PC) */
    if (!check_extensions(sf, "ktsl2asbin,asbin"))
        goto fail;

    /* KTSR can be a memory file (ktsl2asbin), streams (ktsl2stbin) and global config (ktsl2gcbin)
     * This accepts .ktsl2asbin with internal data or external streams as subsongs.
     * Some info from KTSR.bt */

    if (target_subsong == 0) target_subsong = 1;
    ktsr.target_subsong = target_subsong;

    if (!parse_ktsr(&ktsr, sf))
        goto fail;

    /* open companion body */
    if (ktsr.is_external) {
        const char* companion_ext = check_extensions(sf, "asbin") ? "stbin" : "ktsl2stbin";
        sf_b = open_streamfile_by_ext(sf, companion_ext);
        if (!sf_b) {
            vgm_logi("KTSR: companion file '*.%s' not found\n", companion_ext);
            goto fail;
        }
    }
    else {
        sf_b = sf;
    }


    /* subfiles */
    {
        VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf) = NULL;
        const char* ext;
        switch(ktsr.codec) {
            case RIFF_ATRAC9:   init_vgmstream = init_vgmstream_riff; ext = "at9"; break;
            case KOVS:          init_vgmstream = init_vgmstream_ogg_vorbis; ext = "kvs"; break;
            case KTSS:          init_vgmstream = init_vgmstream_ktss; ext = "ktss"; break;
            default: break;
        }

        if (init_vgmstream) {
            vgmstream = init_vgmstream_ktsr_sub(sf_b, &ktsr, init_vgmstream, ext);
            if (!vgmstream) goto fail;

            if (sf_b != sf) close_streamfile(sf_b);
            return vgmstream;
        }
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ktsr.channels, ktsr.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KTSR;
    vgmstream->sample_rate = ktsr.sample_rate;
    vgmstream->num_samples = ktsr.num_samples;
    vgmstream->loop_start_sample = ktsr.loop_start;
    vgmstream->loop_end_sample = ktsr.num_samples;
    vgmstream->stream_size = ktsr.stream_sizes[0];
    vgmstream->num_streams = ktsr.total_subsongs;
    vgmstream->channel_layout = ktsr.channel_layout;
    strcpy(vgmstream->stream_name, ktsr.name);

    switch(ktsr.codec) {

        case MSADPCM:
            vgmstream->coding_type = coding_MSADPCM_int;
            vgmstream->layout_type = layout_none;
            separate_offsets = 1;

            /* 0x00: samples per frame */
            vgmstream->frame_size = read_u16le(ktsr.extra_offset + 0x02, sf_b);
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            separate_offsets = 1;

            dsp_read_coefs_le(vgmstream, sf, ktsr.extra_offset + 0x1c, 0x60);
            dsp_read_hist_le (vgmstream, sf, ktsr.extra_offset + 0x40, 0x60);
            break;

#ifdef VGM_USE_ATRAC9
        case ATRAC9: {
            /* 0x00: samples per frame */
            /* 0x02: frame size */
            uint32_t config_data = read_u32be(ktsr.extra_offset + 0x04, sf);
            if ((config_data & 0xFF) == 0xFE) /* later versions(?) in LE */
                config_data = read_u32le(ktsr.extra_offset + 0x04, sf);

            vgmstream->layout_data = build_layered_atrac9(&ktsr, sf_b, config_data);
            if (!vgmstream->layout_data) goto fail;
            vgmstream->layout_type = layout_layered;
            vgmstream->coding_type = coding_ATRAC9;
            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream_bf(vgmstream, sf_b, ktsr.stream_offsets[0], 1))
        goto fail;


    /* data offset per channel is absolute (not actual interleave since there is padding) in some cases */
    if (separate_offsets) {
        int i;
        for (i = 0; i < ktsr.channels; i++) {
            vgmstream->ch[i].offset = ktsr.stream_offsets[i];
        }
    }

    if (sf_b != sf) close_streamfile(sf_b);
    return vgmstream;

fail:
    if (sf_b != sf) close_streamfile(sf_b);
    close_vgmstream(vgmstream);
    return NULL;
}

// TODO improve, unity with other metas that do similar stuff
static VGMSTREAM* init_vgmstream_ktsr_sub(STREAMFILE* sf_b, ktsr_header* ktsr, VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf), const char* ext) {
    VGMSTREAM* sub_vgmstream = NULL;
    STREAMFILE* temp_sf = setup_subfile_streamfile(sf_b, ktsr->stream_offsets[0], ktsr->stream_sizes[0], ext);
    if (!temp_sf) return NULL;

    sub_vgmstream = init_vgmstream(temp_sf);
    close_streamfile(temp_sf);
    if (!sub_vgmstream) return NULL;

    sub_vgmstream->stream_size = ktsr->stream_sizes[0];
    sub_vgmstream->num_streams = ktsr->total_subsongs;
    sub_vgmstream->channel_layout = ktsr->channel_layout;

    strcpy(sub_vgmstream->stream_name, ktsr->name);

    return sub_vgmstream;
}


static layered_layout_data* build_layered_atrac9(ktsr_header* ktsr, STREAMFILE* sf, uint32_t config_data) {
    STREAMFILE* temp_sf = NULL;
    layered_layout_data* data = NULL;
    int layers = ktsr->channels;
    int i;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    for (i = 0; i < layers; i++) {
        data->layers[i] = allocate_vgmstream(1, 0);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = ktsr->sample_rate;
        data->layers[i]->num_samples = ktsr->num_samples;

#ifdef VGM_USE_ATRAC9
        {
            atrac9_config cfg = {0};

            cfg.config_data = config_data;
            cfg.channels = 1;
            cfg.encoder_delay = 256; /* observed default (ex. Attack on Titan PC vs Vita) */

            data->layers[i]->codec_data = init_atrac9(&cfg);
            if (!data->layers[i]->codec_data) goto fail;
            data->layers[i]->coding_type = coding_ATRAC9;
            data->layers[i]->layout_type = layout_none;
        }
#else
        goto fail;
#endif

        temp_sf = setup_subfile_streamfile(sf, ktsr->stream_offsets[i], ktsr->stream_sizes[i], NULL);
        if (!temp_sf) goto fail;

        if (!vgmstream_open_stream(data->layers[i], temp_sf, 0x00))
            goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    return data;
fail:
    close_streamfile(temp_sf);
    free_layout_layered(data);
    return NULL;
}


static int parse_codec(ktsr_header* ktsr) {

    /* platform + format to codec, simplified until more codec combos are found */
    switch(ktsr->platform) {
        case 0x01: /* PC */
            if (ktsr->is_external) {
                if (ktsr->format == 0x0005)
                    ktsr->codec = KOVS; // Atelier Ryza (PC)
                else
                    goto fail;
            }
            else if (ktsr->format == 0x0000) {
                ktsr->codec = MSADPCM; // Warrior Orochi 4 (PC)
            }
            else {
                goto fail;
            }
            break;

        case 0x03: /* PS4/VITA */
            if (ktsr->is_external) {
                if (ktsr->format == 0x1001)
                    ktsr->codec = RIFF_ATRAC9; // Nioh (PS4)
                else
                    goto fail;
            }
            else if (ktsr->format == 0x0001)
                ktsr->codec = ATRAC9; // Attack on Titan: Wings of Freedom (Vita)
            else
                goto fail;
            break;

        case 0x04: /* Switch */
            if (ktsr->is_external) {
                if (ktsr->format == 0x0005)
                    ktsr->codec = KTSS; // [Ultra Kaiju Monster Rancher (Switch)]
                else
                    goto fail;
            }
            else if (ktsr->format == 0x0000)
                ktsr->codec = DSP; // Fire Emblem: Three Houses (Switch)
            else
                goto fail;
            break;

        default:
            goto fail;
    }

    return 1;
fail:
    VGM_LOG("ktsr: unknown codec combo: external=%x, format=%x, platform=%x\n", ktsr->is_external, ktsr->format, ktsr->platform);
    return 0;
}

static int parse_ktsr_subfile(ktsr_header* ktsr, STREAMFILE* sf, uint32_t offset) {
    uint32_t suboffset, starts_offset, sizes_offset;
    int i;
    uint32_t type;

    type = read_u32be(offset + 0x00, sf); /* hash-id? */
  //size = read_u32le(offset + 0x04, sf);

    /* probably could check the flag in sound header, but the format is kinda messy */
    switch(type) {

        case 0x38D0437D: /* external [Nioh (PC/PS4), Atelier Ryza (PC)] */
        case 0x3DEA478D: /* external [Nioh (PC)] (smaller) */
        case 0xDF92529F: /* external [Atelier Ryza (PC)] */
        case 0x6422007C: /* external [Atelier Ryza (PC)] */
            /* 08 subtype? (ex. 0x522B86B9)
             * 0c channels
             * 10 ? (always 0x002706B8)
             * 14 external codec
             * 18 sample rate
             * 1c num samples
             * 20 null?
             * 24 loop start or -1 (loop end is num samples)
             * 28 channel layout (or null?)
             * 2c null
             * 30 null
             * 34 data offset (absolute to external stream, points to actual format and not to mini-header)
             * 38 data size
             * 3c always 0x0200
             */

            ktsr->channels  = read_u32le(offset + 0x0c, sf);
            ktsr->format    = read_u32le(offset + 0x14, sf);
            /* other fields will be read in the external stream */

            ktsr->channel_layout = read_u32le(offset + 0x28, sf);

            if (type == 0x3DEA478D) { /* Nioh (PC) has one less field, some files only [ABS.ktsl2asbin] */
                ktsr->stream_offsets[0] = read_u32le(offset + 0x30, sf);
                ktsr->stream_sizes[0]   = read_u32le(offset + 0x34, sf);
            }
            else {
                ktsr->stream_offsets[0] = read_u32le(offset + 0x34, sf);
                ktsr->stream_sizes[0]   = read_u32le(offset + 0x38, sf);
            }
            ktsr->is_external = 1;

            break;

        case 0x41FDBD4E: /* internal [Attack on Titan: Wings of Freedom (Vita)] */
        case 0x6FF273F9: /* internal [Attack on Titan: Wings of Freedom (PC/Vita)] */
        case 0x6FCAB62E: /* internal [Marvel Ultimate Alliance 3: The Black Order (Switch)] */
        case 0x6AD86FE9: /* internal [Atelier Ryza (PC/Switch), Persona5 Scramble (Switch)] */
        case 0x10250527: /* internal [Fire Emblem: Three Houses DLC (Switch)] */
            /* 08 subtype? (0x6029DBD2, 0xD20A92F90, 0xDC6FF709)
             * 0c channels
             * 10 format? (00=platform's ADPCM? 01=ATRAC9?)
             * 11 bps? (always 16)
             * 12 null
             * 14 sample rate
             * 18 num samples
             * 1c null or 0x100?
             * 20 loop start or -1 (loop end is num samples)
             * 24 channel layout or null
             * 28 header offset (within subfile)
             * 2c header size [B, C]
             * 30 offset to data start offset [A, C] or to data start+size [B]
             * 34 offset to data size [A, C] or same per channel
             * 38 always 0x0200
             * -- header
             * -- data start offset
             * -- data size
             */

            ktsr->channels      = read_u32le(offset + 0x0c, sf);
            ktsr->format        = read_u8   (offset + 0x10, sf);
            ktsr->sample_rate   = read_s32le(offset + 0x14, sf);
            ktsr->num_samples   = read_s32le(offset + 0x18, sf);
            ktsr->loop_start    = read_s32le(offset + 0x20, sf);
            ktsr->channel_layout= read_u32le(offset + 0x24, sf);
            ktsr->extra_offset  = read_u32le(offset + 0x28, sf) + offset;
            if (type == 0x41FDBD4E || type == 0x6FF273F9) /* v1 */
                suboffset = offset + 0x2c;
            else
                suboffset = offset + 0x30;

            if (ktsr->channels > MAX_CHANNELS) {
                VGM_LOG("ktsr: max channels found\n");
                goto fail;
            }

            starts_offset = read_u32le(suboffset + 0x00, sf) + offset;
            sizes_offset  = read_u32le(suboffset + 0x04, sf) + offset;
            for (i = 0; i < ktsr->channels; i++) {
                ktsr->stream_offsets[i] = read_u32le(starts_offset + 0x04*i, sf) + offset;
                ktsr->stream_sizes[i]   = read_u32le(sizes_offset  + 0x04*i, sf);
            }

            ktsr->loop_flag = (ktsr->loop_start >= 0);

            break;

        default:
            /* streams also have their own chunks like 0x09D4F415, not needed here */
            VGM_LOG("ktsr: unknown subheader at %x\n", offset);
            goto fail;
    }

    if (!parse_codec(ktsr))
        goto fail;

    return 1;
fail:
    VGM_LOG("ktsr: error parsing subheader\n");
    return 0;
}

static void build_name(ktsr_header* ktsr, STREAMFILE* sf) {
    char sound_name[255] = {0};
    char config_name[255] = {0};

    /* names can be different or same but usually config is better */
    if (ktsr->sound_name_offset) {
        read_string(sound_name, sizeof(sound_name), ktsr->sound_name_offset, sf);
    }
    if (ktsr->config_name_offset) {
        read_string(config_name, sizeof(config_name), ktsr->config_name_offset, sf);
    }

    //if (longname[0] && shortname[0]) {
    //    snprintf(ktsr->name, sizeof(ktsr->name), "%s; %s", longname, shortname);
    //}
    if (config_name[0]) {
        snprintf(ktsr->name, sizeof(ktsr->name), "%s", config_name);

    }
    else if (sound_name[0]) {
        snprintf(ktsr->name, sizeof(ktsr->name), "%s", sound_name);
    }

}

static void parse_longname(ktsr_header* ktsr, STREAMFILE* sf, uint32_t target_id) {
    /* more configs than sounds is possible so we need target_id first */
    uint32_t offset, end, name_offset;
    uint32_t stream_id;

    offset = 0x40;
    end = get_streamfile_size(sf);
    while (offset < end) {
        uint32_t type = read_u32be(offset + 0x00, sf); /* hash-id? */
        uint32_t size = read_u32le(offset + 0x04, sf);
        switch(type) {
            case 0xBD888C36: /* config */
                stream_id = read_u32be(offset + 0x08, sf);
                if (stream_id != target_id)
                    break;

                name_offset = read_u32le(offset + 0x28, sf);
                if (name_offset > 0)
                    ktsr->config_name_offset = offset + name_offset;
                return; /* id found */

            default:
                break;
        }

        offset += size;
    }
}

static int parse_ktsr(ktsr_header* ktsr, STREAMFILE* sf) {
    uint32_t offset, end, header_offset, name_offset;
    uint32_t stream_id = 0, stream_count;

    /* 00: KTSR
     * 04: type
     * 08: version?
     * 0a: unknown (usually 00, 02/03 seen in Vita)
     * 0b: platform (01=PC, 03=Vita, 04=Switch)
     * 0c: game id?
     * 10: null
     * 14: null
     * 18: file size
     * 1c: file size
     * up to 40: reserved
     * until end: entries (totals not defined) */

    ktsr->platform = read_u8(0x0b,sf);

    if (read_u32le(0x18, sf) != read_u32le(0x1c, sf))
        goto fail;
    if (read_u32le(0x1c, sf) != get_streamfile_size(sf))
        goto fail;

    offset = 0x40;
    end = get_streamfile_size(sf);
    while (offset < end) {
        uint32_t type = read_u32be(offset + 0x00, sf); /* hash-id? */
        uint32_t size = read_u32le(offset + 0x04, sf);

        /* parse chunk-like subfiles, usually N configs then N songs */
        switch(type) {
            case 0x6172DBA8: /* padding (empty) */
            case 0xBD888C36: /* config (floats, stream id, etc, may have extended name) */
            case 0xC9C48EC1: /* unknown (has some string inside like "boss") */
            case 0xA9D23BF1: /* "state container", some kind of config/floats, witgh names like 'State_bgm01'..N */
            case 0x836FBECA: /* unknown (~0x300, encrypted? table + data) */
                break;

            case 0xC5CCCB70: /* sound (internal data or external stream) */
                ktsr->total_subsongs++;

                /* sound table:
                 * 08: stream id (used in several places)
                 * 0c: unknown (low number but not version?)
                 * 0e: external flag
                 * 10: sub-streams?
                 * 14: offset to header offset
                 * 18: offset to name
                 * --: name
                 * --: header offset
                 * --: header
                 * --: subheader (varies) */


                if (ktsr->total_subsongs == ktsr->target_subsong) {

                    stream_id = read_u32be(offset + 0x08,sf);
                    //ktsr->is_external = read_u16le(offset + 0x0e,sf);
                    stream_count = read_u32le(offset + 0x10,sf);
                    if (stream_count != 1) {
                        VGM_LOG("ktsr: unknown stream count\n");
                        goto fail;
                    }

                    header_offset = read_u32le(offset + 0x14, sf);
                    name_offset   = read_u32le(offset + 0x18, sf);
                    if (name_offset > 0)
                        ktsr->sound_name_offset = offset + name_offset;

                    header_offset = read_u32le(offset + header_offset, sf) + offset;

                    if (!parse_ktsr_subfile(ktsr, sf, header_offset))
                        goto fail;
                }
                break;

            default:
                /* streams also have their own chunks like 0x09D4F415, not needed here */  
                VGM_LOG("ktsr: unknown chunk at %x\n", offset);
                goto fail;
        }

        offset += size;
    }

    if (ktsr->target_subsong > ktsr->total_subsongs)
        goto fail;

    parse_longname(ktsr, sf, stream_id);
    build_name(ktsr, sf);

    return 1;
fail:
    vgm_logi("KTSR: unknown variation (report)\n");
    return 0;
}
