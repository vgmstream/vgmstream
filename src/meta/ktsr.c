#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/companion_files.h"
#include "ktsr_streamfile.h"

typedef enum { NONE, MSADPCM, DSP, GCADPCM, ATRAC9, RIFF_ATRAC9, KMA9, AT9_KM9, KOVS, KTSS, KTAC, KA1A, KA1A_INTERNAL, } ktsr_codec;

#define MAX_CHANNELS 8

typedef struct {
    bool is_srsa;
    bool is_sdbs;
    uint32_t as_offset;
    uint32_t as_size;
    uint32_t st_offset;
    uint32_t st_size;
} ktsr_meta_t;

typedef struct {
    uint32_t as_offset;
    uint32_t as_size;

    int total_subsongs;
    int target_subsong;
    ktsr_codec codec;

    uint32_t audio_id;
    int platform;
    int format;
    uint32_t codec_value;
    uint32_t sound_id;
    uint32_t sound_flags;
    uint32_t config_flags;

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
} ktsr_header_t;

static VGMSTREAM* init_vgmstream_ktsr_internal(STREAMFILE* sf, ktsr_meta_t* info);
static bool parse_ktsr(ktsr_header_t* ktsr, STREAMFILE* sf);
static layered_layout_data* build_layered_atrac9(ktsr_header_t* ktsr, STREAMFILE *sf, uint32_t config_data);
static VGMSTREAM* init_vgmstream_ktsr_sub(STREAMFILE* sf_b, uint32_t st_offset, ktsr_header_t* ktsr, init_vgmstream_t init_vgmstream, const char* ext);

/* KTSR - Koei Tecmo sound resource container (KTSL2 sound lib) */
VGMSTREAM* init_vgmstream_ktsr(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "KTSR"))
        return NULL;
    /* others: see internal */

    /* .ktsl2asbin: common [Atelier Ryza (PC/Switch), Nioh (PC)] 
     * .asbin: Warriors Orochi 4 (PC) */
    if (!check_extensions(sf, "ktsl2asbin,asbin"))
        return NULL;

    ktsr_meta_t info = {
        .as_size    = get_streamfile_size(sf),
    };
    return init_vgmstream_ktsr_internal(sf, &info);
}

/* ASRS - container of KTSR found in newer games */
VGMSTREAM* init_vgmstream_asrs(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "ASRS"))
        return NULL;
    /* 0x04: null */
    /* 0x08: file size */
    /* 0x0c: null */

    /* .srsa: header id and 'class' in hashed names */
    if (!check_extensions(sf, "srsa"))
        return NULL;

    /* mini-container of memory-KTSR (streams also have a "TSRS" for stream-KTSR).
     * .srsa/srst usually have hashed filenames, so it isn't easy to match them, so this
     * is mainly useful for .srsa with internal streams. */

    ktsr_meta_t info = {
        .is_srsa = true,
        .as_offset  = 0x10,
        .as_size    = get_streamfile_size(sf) - 0x10,
        .st_offset  = 0x10,
    };
    return init_vgmstream_ktsr_internal(sf, &info);
}

/* sdbs - container of KTSR found in newer games */
VGMSTREAM* init_vgmstream_sdbs(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "sdbs"))
        return NULL;

    // .srsa: actual extension
    if (!check_extensions(sf, "k2sb"))
        return NULL;

    // mini-container of memory + stream KTSR
    ktsr_meta_t info = {
        .is_sdbs    = true,
        .as_offset  = read_u32le(0x04, sf),
        .as_size    = read_u32le(0x08, sf),
        .st_offset  = read_u32le(0x0c, sf),
        .st_size    = read_u32le(0x10, sf),
    };
    return init_vgmstream_ktsr_internal(sf, &info);
}


static STREAMFILE* setup_sf_body(STREAMFILE* sf, ktsr_header_t* ktsr, ktsr_meta_t* info) {

    // use current
    if (!ktsr->is_external)
        return sf;
   
    // skip extra header (internals are pre-adjusted) */
    if (ktsr->is_external && info->st_offset) {
        for (int i = 0; i < ktsr->channels; i++) {
            ktsr->stream_offsets[i] += info->st_offset;
        }

        //ktsr->extra_offset += ktsr->st_offset; // ?
    }

    if (info->is_sdbs) {
        // .k2sb have data pasted together
        return sf;
    }


    /* open companion body */
    STREAMFILE* sf_b = NULL;
    if (info->is_srsa) {
        // try parsing TXTM if present, since .srsa+srst have hashed names and don't match unless renamed
        sf_b = read_filemap_file(sf, 0);
    }

    if (!sf_b) {
        // try (name).(ext), as seen in older games
        const char* companion_ext = check_extensions(sf, "asbin") ? "stbin" : "ktsl2stbin";
        if (info->is_srsa)
            companion_ext = "srst";

        sf_b = open_streamfile_by_ext(sf, companion_ext);
        if (!sf_b) {
            vgm_logi("KTSR: companion file '*.%s' not found\n", companion_ext);
            return NULL;
        }
    }

    return sf_b;
}


static VGMSTREAM* init_vgmstream_ktsr_internal(STREAMFILE* sf, ktsr_meta_t* info) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_b = NULL;
    ktsr_header_t ktsr = {0};


    ktsr.as_offset = info->as_offset;
    ktsr.as_size = info->as_size;

    /* checks */
    if (!is_id32be(ktsr.as_offset + 0x00, sf, "KTSR"))
        return NULL;
    if (read_u32be(ktsr.as_offset + 0x04, sf) != 0x777B481A) /* hash id: 0x777B481A=as, 0x0294DDFC=st, 0xC638E69E=gc */
        return NULL;

    bool separate_offsets = false;
    int target_subsong = sf->stream_index;

    /* KTSR can be a memory file (ktsl2asbin), streams (ktsl2stbin) and global config (ktsl2gcbin)
     * This accepts .ktsl2asbin with internal data or external streams as subsongs.
     * Hashes are meant to be read LE, but here are BE for easier comparison (they probably correspond
     * to some text but are pre-hashed in exes). Some info from KTSR.bt */

    if (target_subsong == 0) target_subsong = 1;
    ktsr.target_subsong = target_subsong;

    if (!parse_ktsr(&ktsr, sf))
        return NULL;

    if (ktsr.total_subsongs == 0) {
        vgm_logi("KTSR: file has no subsongs (ignore)\n");
        return NULL;
    }


    sf_b = setup_sf_body(sf, &ktsr, info);
    if (!sf_b) goto fail;

    /* subfiles */
    {
        // autodetect ill-defined streams (assumes file isn't encrypted)
        if (ktsr.codec == AT9_KM9) {
            if (is_id32be(ktsr.stream_offsets[0], sf_b, "KMA9"))
                ktsr.codec = KMA9;
            else
                ktsr.codec = RIFF_ATRAC9;
        }

        init_vgmstream_t init_vgmstream = NULL;
        const char* ext;
        switch(ktsr.codec) {
            case RIFF_ATRAC9:   init_vgmstream = init_vgmstream_riff; ext = "at9"; break;       // Nioh (PS4)
            case KOVS:          init_vgmstream = init_vgmstream_ogg_vorbis; ext = "kvs"; break; // Nioh (PC), Fairy Tail 2 (PC)
            case KTSS:          init_vgmstream = init_vgmstream_ktss; ext = "ktss"; break;      // 
            case KTAC:          init_vgmstream = init_vgmstream_ktac; ext = "ktac"; break;      // Blue Reflection Tie (PS4)
            case KA1A:          init_vgmstream = init_vgmstream_ka1a; ext = "ka1a"; break;      // Dynasty Warriors Origins (PC)
            case KMA9:          init_vgmstream = init_vgmstream_kma9; ext = "km9"; break;       // Fairy Tail 2 (PS4)
            default: break;
        }

        if (init_vgmstream) {
            vgmstream = init_vgmstream_ktsr_sub(sf_b, info->st_offset, &ktsr, init_vgmstream, ext);
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
            vgmstream->coding_type = coding_MSADPCM_mono;
            vgmstream->layout_type = layout_none;
            separate_offsets = true;

            /* 0x00: samples per frame */
            vgmstream->frame_size = read_u16le(ktsr.extra_offset + 0x02, sf_b);
            break;

        case KA1A_INTERNAL: {
            // 00: bitrate mode
            // XX: start offsets per channel (from hash-id start aka extra_offset - 0x48)
            // XX: size per channel
            // XX: padding

            int bitrate_mode = read_s32le(ktsr.extra_offset + 0x00, sf); // signed! (may be negative)

            vgmstream->codec_data = init_ka1a(bitrate_mode, ktsr.channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_KA1A;
            vgmstream->layout_type = layout_none;

            // mono streams handled in decoder, though needs channel offsets + flag
            vgmstream->codec_config = 1;
            separate_offsets = true;

            break;
        }

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            separate_offsets = true;

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
        for (int i = 0; i < ktsr.channels; i++) {
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

// TODO improve, unify with other metas that do similar stuff
static VGMSTREAM* init_vgmstream_ktsr_sub(STREAMFILE* sf_b, uint32_t st_offset, ktsr_header_t* ktsr, init_vgmstream_t init_vgmstream, const char* ext) {
    VGMSTREAM* sub_vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    temp_sf = setup_ktsr_streamfile(sf_b, st_offset, ktsr->is_external, ktsr->stream_offsets[0], ktsr->stream_sizes[0], ext);
    if (!temp_sf) return NULL;

    sub_vgmstream = init_vgmstream(temp_sf);
    close_streamfile(temp_sf);
    if (!sub_vgmstream) {
        VGM_LOG("ktsr: can't open subfile %s at %x (size %x)\n", ext, ktsr->stream_offsets[0], ktsr->stream_sizes[0]);
        return NULL;
    }

    sub_vgmstream->stream_size = ktsr->stream_sizes[0];
    sub_vgmstream->num_streams = ktsr->total_subsongs;
    sub_vgmstream->channel_layout = ktsr->channel_layout;

    strcpy(sub_vgmstream->stream_name, ktsr->name);

    return sub_vgmstream;
}


static layered_layout_data* build_layered_atrac9(ktsr_header_t* ktsr, STREAMFILE* sf, uint32_t config_data) {
    STREAMFILE* temp_sf = NULL;
    layered_layout_data* data = NULL;
    int layers = ktsr->channels;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    for (int i = 0; i < layers; i++) {
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


static int parse_codec(ktsr_header_t* ktsr) {

    /* platform + format to codec, simplified until more codec combos are found */
    switch(ktsr->platform) {
        case 0x01: /* PC */
        case 0x05: /* PC/Steam, Android [Fate/Samurai Remnant (PC)] */
            if (ktsr->format == 0x0000 && !ktsr->is_external)
                ktsr->codec = MSADPCM; // Warrior Orochi 4 (PC)
            else if (ktsr->format == 0x0001)
                ktsr->codec = KA1A_INTERNAL; // Dynasty Warriors Origins (PC)
            else if (ktsr->format == 0x0005 && ktsr->is_external && ktsr->codec_value == 0x0840)
                ktsr->codec = KTAC; // Shin Hokuto Musou (Android
            else if (ktsr->format == 0x0005 && ktsr->is_external)
                ktsr->codec = KOVS; // Atelier Ryza (PC)
            else if (ktsr->format == 0x1001 && ktsr->is_external)
                ktsr->codec = KA1A; // Dynasty Warriors Origins (PC)
            else
                goto fail;
            break;

        case 0x03: /* PS4/VITA */
            if (ktsr->format == 0x0001 && !ktsr->is_external)
                ktsr->codec = ATRAC9; // Attack on Titan: Wings of Freedom (Vita)
            else if (ktsr->format == 0x0005 && ktsr->is_external)
                ktsr->codec = KTAC; // Blue Reflection Tie (PS4)
            else if (ktsr->format == 0x1001 && ktsr->is_external)
                ktsr->codec = AT9_KM9; // Nioh (PS4)-at9, Fairy Tail 2 (PS4)-km9 (no apparent differences of flags/channels/etc)
            else
                goto fail;
            break;

        case 0x04: /* Switch */
            if (ktsr->format == 0x0000 && !ktsr->is_external)
                ktsr->codec = DSP; // [Fire Emblem: Three Houses (Switch)]
            else if (ktsr->format == 0x0005 && ktsr->is_external)
                ktsr->codec = KTSS; // [Ultra Kaiju Monster Rancher (Switch)]
            else if (ktsr->format == 0x1000 && ktsr->is_external)
                ktsr->codec = KTSS; // [Fire Emblem: Three Houses (Switch)-some DSP voices]
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

static bool parse_ktsr_subfile(ktsr_header_t* ktsr, STREAMFILE* sf, uint32_t offset) {
    uint32_t suboffset, starts_offset, sizes_offset;

    uint32_t type = read_u32be(offset + 0x00, sf); /* hash-id? */
  //size = read_u32le(offset + 0x04, sf);

    // probably could check the flags in sound header, but the format is kinda messy
    // (all these numbers are surely LE hashes of something)
    switch(type) {

        case 0x38D0437D: /* external [Nioh (PC/PS4), Atelier Ryza (PC)] */
        case 0x3DEA478D: /* external [Nioh (PC)] (smaller) */
        case 0xDF92529F: /* external [Atelier Ryza (PC)] */
        case 0x6422007C: /* external [Atelier Ryza (PC)] */
        case 0x793A1FD7: /* external [Stranger of Paradise (PS4)]-encrypted */
        case 0xA0F4FC6C: /* external [Stranger of Paradise (PS4)]-encrypted */
            /* 08 subtype? (ex. 0x522B86B9)
             * 0c channels
             * 10 ? (always 0x002706B8 / 7864523D in SoP)
             * 14 external codec
             * 18 sample rate
             * 1c num samples
             * 20 null or codec-related value (RIFF_AT9/KM9=0x100, KTAC=0x840)
             * 24 loop start or -1 (loop end is num samples)
             * 28 channel layout (or null?)
             * 2c null
             * 30 null
             * 34 data offset (absolute to external stream, points to actual format and not to mini-header)
             * 38 data size
             * 3c always 0x0200
             */
            //;VGM_LOG("header %08x at %x\n", type, offset);

            ktsr->channels      = read_u32le(offset + 0x0c, sf);
            ktsr->format        = read_u32le(offset + 0x14, sf);
            ktsr->codec_value   = read_u32le(offset + 0x20, sf);
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
            ktsr->is_external = true;
            break;

        case 0x41FDBD4E: /* internal [Attack on Titan: Wings of Freedom (Vita)] */
        case 0x6FF273F9: /* internal [Attack on Titan: Wings of Freedom (PC/Vita)] */
        case 0x6FCAB62E: /* internal [Marvel Ultimate Alliance 3: The Black Order (Switch)] */
        case 0x6AD86FE9: /* internal [Atelier Ryza (PC/Switch), Persona5 Scramble (Switch)] */
        case 0x10250527: /* internal [Fire Emblem: Three Houses DLC (Switch)] */
            /* 08 subtype? (0x6029DBD2, 0xD20A92F90, 0xDC6FF709)
             * 0c channels
             * 10 format
             * 11 null or sometimes 16
             * 12 null
             * 14 sample rate
             * 18 num samples
             * 1c null or codec-related value?
             * 20 loop start or -1 (loop end is num samples)
             * 24 null or channel layout (for 1 track in case of multi-track streams)
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
            for (int i = 0; i < ktsr->channels; i++) {
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
    return true;
fail:
    VGM_LOG("ktsr: error parsing subheader\n");
    return false;
}

/* ktsr engine reads+decrypts in the same func based on passed flag tho (reversed from exe)
 * Strings are usually ASCII but Shift-JIS is used in rare cases (0x0c3e2edf.srsa) */
static void decrypt_string_ktsr(char* buf, size_t buf_size, uint32_t seed) {
    for (int i = 0; i < buf_size - 1; i++) {
        if (!buf[i]) /* just in case */
            break;

        seed = 0x343FD * seed + 0x269EC3; /* classic rand */
        buf[i] ^= (seed >> 16) & 0xFF; /* 3rd byte */
        if (!buf[i]) /* end null is also encrypted (but there are extra nulls after it anyway) */
            break;
    }
}

/* like read_string but allow any value since it can be encrypted */
static size_t read_string_ktsr(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    int pos;

    for (pos = 0; pos < buf_size; pos++) {
        uint8_t byte = read_u8(offset + pos, sf);
        char c = (char)byte;
        if (buf) buf[pos] = c;
        if (c == '\0')
            return pos;
        if (pos+1 == buf_size) {
            if (buf) buf[pos] = '\0';
            return buf_size;
        }
    }

    return 0;
}

static void build_name(ktsr_header_t* ktsr, STREAMFILE* sf) {
    char sound_name[255] = {0};
    char config_name[255] = {0};

    if (ktsr->sound_name_offset) {
        read_string_ktsr(sound_name, sizeof(sound_name), ktsr->sound_name_offset, sf);
        if (ktsr->sound_flags & 0x0008)
            decrypt_string_ktsr(sound_name, sizeof(sound_name), ktsr->audio_id);
    }

    if (ktsr->config_name_offset) {
        read_string_ktsr(config_name, sizeof(config_name), ktsr->config_name_offset, sf);
        if (ktsr->config_flags & 0x0200)
            decrypt_string_ktsr(config_name, sizeof(config_name), ktsr->audio_id);
    }

    // names can be different or same but usually config name is better
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

static void parse_longname(ktsr_header_t* ktsr, STREAMFILE* sf) {
    /* more configs than sounds is possible so we need target_id first */
    uint32_t offset, end, name_offset;
    uint32_t stream_id;

    offset = 0x40 + ktsr->as_offset;
    end = ktsr->as_offset + ktsr->as_size;
    while (offset < end) {
        uint32_t type = read_u32be(offset + 0x00, sf); /* hash-id? */
        uint32_t size = read_u32le(offset + 0x04, sf);
        switch(type) {
            case 0xBD888C36: /* config */
                stream_id = read_u32le(offset + 0x08, sf);
                if (stream_id != ktsr->sound_id)
                    break;

                ktsr->config_flags = read_u32le(offset + 0x0c, sf);

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

static bool parse_ktsr(ktsr_header_t* ktsr, STREAMFILE* sf) {
    uint32_t offset, end, header_offset, name_offset;
    uint32_t stream_count;

    /* 00: KTSR
     * 04: type
     * 08: version?
     * 0a: unknown (usually 00, 02/03 seen in Vita)
     * 0b: platform
     * 0c: audio id? (seen in multiple files/games and used as Ogg stream IDs)
     * 10: null
     * 14: null
     * 18: file size
     * 1c: file size
     * up to 40: reserved
     * until end: entries (totals not defined) */

    ktsr->platform = read_u8   (ktsr->as_offset + 0x0b,sf);
    ktsr->audio_id = read_u32le(ktsr->as_offset + 0x0c,sf);

    if (read_u32le(ktsr->as_offset + 0x18, sf) != read_u32le(ktsr->as_offset + 0x1c, sf))
        goto fail;
    if (read_u32le(ktsr->as_offset + 0x1c, sf) != ktsr->as_size) {
        vgm_logi("KTSR: incorrect file size (bad rip?)\n");
        goto fail;
    }

    offset = 0x40 + ktsr->as_offset;
    end = ktsr->as_offset + ktsr->as_size;
    while (offset < end) {
        uint32_t type = read_u32be(offset + 0x00, sf); /* hash-id? */
        uint32_t size = read_u32le(offset + 0x04, sf);

        /* parse chunk-like subfiles, usually N configs then N songs */
        switch(type) {
            case 0x6172DBA8: /* ? (mostly empty) */
            case 0xBD888C36: /* cue? (floats, stream id, etc, may have extended name; can have sub-chunks)-appears N times */
            case 0xC9C48EC1: /* unknown (has some string inside like "boss") */
            case 0xA9D23BF1: /* "state container", some kind of config/floats, with names like 'State_bgm01'..N */
            case 0x836FBECA: /* random sfxs? (ex. weapon sfx variations; IDs + encrypted name table + data) */
            case 0x2d232c98: /* big mix of tables, found in DWO BGM srsa */
                break;

            case 0xC5CCCB70: /* sound (internal data or external stream) */
                ktsr->total_subsongs++;

                /* sound table:
                 * 08: current/stream id (used in several places)
                 * 0c: flags (sounds only; other types are similar but different bits)
                 *   0x08: encrypted names (only used after ASRS was introduced?)
                 *   0x10000: external flag
                 * 10: sub-streams?
                 * 14: offset to header offset
                 * 18: offset to name
                 * --: name
                 * --: header offset
                 * --: header
                 * --: subheader (varies) */

                if (ktsr->total_subsongs == ktsr->target_subsong) {

                    ktsr->sound_id = read_u32le(offset + 0x08,sf);
                    ktsr->sound_flags = read_u32le(offset + 0x0c,sf);
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
                VGM_LOG("ktsr: unknown chunk 0x%08x at %x\n", type, offset);
                goto fail;
        }

        offset += size;
    }

    if (ktsr->total_subsongs == 0) {
        return true;
    }

    if (ktsr->target_subsong > ktsr->total_subsongs)
        goto fail;

    parse_longname(ktsr, sf);
    build_name(ktsr, sf);

    return true;
fail:
    vgm_logi("KTSR: unknown variation (report)\n");
    return false;
}
