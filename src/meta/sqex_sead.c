#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "sqex_streamfile.h"


typedef struct {
    int big_endian;

    int is_sab;
    int is_mab;

    int total_subsongs;
    int target_subsong;

    uint16_t mtrl_index;
    uint16_t mtrl_number;
    int loop_flag;
    int channels;
    int codec;
    int sample_rate;
    int loop_start;
    int loop_end;
    uint32_t mtrl_offset;
    uint32_t extradata_offset;
    uint32_t extradata_size;
    uint32_t stream_size;
    uint16_t extradata_id;

    uint32_t filename_offset;
    uint32_t filename_size;
    uint32_t muscname_offset;
    uint32_t muscname_size;
    uint32_t sectname_offset;
    uint32_t sectname_size;
    uint32_t modename_offset;
    uint32_t modename_size;
    uint32_t instname_offset;
    uint32_t instname_size;
    //uint32_t sndname_offset;
    //uint32_t sndname_size;

    uint32_t sections_offset;
    uint32_t snd_section_offset;
    uint32_t seq_section_offset;
    uint32_t trk_section_offset;
    uint32_t musc_section_offset;
    uint32_t inst_section_offset;
    uint32_t mtrl_section_offset;

    char readable_name[STREAM_NAME_SIZE];

} sead_header_t;

static int parse_sead(sead_header_t* sead, STREAMFILE* sf);


/* SABF/MABF - Square Enix's "sead" audio games [Dragon Quest Builders (PS3), Dissidia Opera Omnia (mobile), FF XV (PS4)] */
VGMSTREAM* init_vgmstream_sqex_sead(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    sead_header_t sead = {0};
    off_t start_offset;
    int target_subsong = sf->stream_index;
    read_u32_t read_u32 = NULL;
    read_u16_t read_u16 = NULL;


    /* checks */
    if (is_id32be(0x00, sf, "sabf")) {
        sead.is_sab = 1;
    }
    else if (is_id32be(0x00, sf, "mabf")) {
        sead.is_mab = 1;
    }
    else {
        goto fail;
    }

    /* .sab: sound/bgm
     * .mab: music
     * .sbin: Dissidia Opera Omnia .sab */
    if (!check_extensions(sf,"sab,mab,sbin"))
        goto fail;

    /* SEAD handles both sab/mab in the same lib (libsead), and other similar files (config, engine, etc).
     * Has some chunks pointing to sections, and each section entry (usually starting with section
     * version/reserved/size) is always padded to 0x10. Most values are unsigned. 
     * 
     * "SEAD Engine" (Square Enix Application on Demand Engine) is/was SQEX's internal middleware (~2006),
     * so it's possible SEAD refers to the whole thing rather than audio, but since .sab/mab audio lib typically goes
     * with other engines it's hard to say if "libsead" is the whole engine but trimmed with only audio functions,
     * or is a separate audio lib derived from this "SEAD Engine". */


    sead.big_endian = guess_endian16(0x06, sf); /* no flag, use size */
    if (sead.big_endian) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }

    sead.target_subsong = target_subsong;

    if (!parse_sead(&sead, sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sead.channels, sead.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = sead.is_sab ? meta_SQEX_SAB : meta_SQEX_MAB;
    vgmstream->sample_rate = sead.sample_rate;
    vgmstream->num_streams = sead.total_subsongs;
    vgmstream->stream_size = sead.stream_size;
    strcpy(vgmstream->stream_name, sead.readable_name);

    switch(sead.codec) {
        case 0x00:  /* NONE */
            vgmstream->coding_type = coding_SILENCE;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = sead.sample_rate;

            start_offset = 0;
            break;

        case 0x01: { /* PCM [Chrono Trigger (PC) sfx] */
            start_offset = sead.extradata_offset + sead.extradata_size;

            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            /* no known extradata */

            vgmstream->num_samples = pcm_bytes_to_samples(sead.stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = sead.loop_start;
            vgmstream->loop_end_sample   = sead.loop_end;
            break;
        }

        case 0x02: { /* MSADPCM [Dragon Quest Builders (Vita) sfx] */
            start_offset = sead.extradata_offset + sead.extradata_size;

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_u16(sead.extradata_offset+0x04, sf);

            /* extradata: */
            /* 0x00: version */
            /* 0x01: reserved */
            /* 0x02: size */
            /* 0x02: frame size */
            /* 0x06: reserved */
            /* 0x08: loop start offset */
            /* 0x0c: loop end offset  */

            /* much like AKBs, loop values are slightly different, probably more accurate
             * (if no loop, loop_end doubles as num_samples) */
            vgmstream->num_samples = msadpcm_bytes_to_samples(sead.stream_size, vgmstream->frame_size, vgmstream->channels);
            vgmstream->loop_start_sample = read_u32(sead.extradata_offset+0x08, sf);
            vgmstream->loop_end_sample   = read_u32(sead.extradata_offset+0x0c, sf);
            break;
        }

#ifdef VGM_USE_VORBIS
        case 0x03: { /* VORBIS (Ogg subfile) [Final Fantasy XV Benchmark sfx (PC)] */
            VGMSTREAM* ogg_vgmstream = NULL;
            ogg_vorbis_meta_info_t ovmi = {0};
            off_t subfile_offset = sead.extradata_offset + sead.extradata_size;

            ovmi.meta_type = vgmstream->meta_type;
            ovmi.total_subsongs = sead.total_subsongs;
            ovmi.stream_size = sead.stream_size;

            /* extradata: */
            /* 0x00: version */
            /* 0x01: reserved */
            /* 0x02: size */
            /* 0x04: loop start offset */
            /* 0x08: loop end offset */
            /* 0x0c: num samples */
            /* 0x10: header size */
            /* 0x14: seek table size */
            /* 0x18: reserved x2 */
            /* 0x20: seek table */

            ogg_vgmstream = init_vgmstream_ogg_vorbis_config(sf, subfile_offset, &ovmi);
            if (ogg_vgmstream) {
                ogg_vgmstream->num_streams = vgmstream->num_streams;
                ogg_vgmstream->stream_size = vgmstream->stream_size;
                strcpy(ogg_vgmstream->stream_name, vgmstream->stream_name);

                close_vgmstream(vgmstream);
                return ogg_vgmstream;
            }
            else {
                goto fail;
            }

            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x04: { /* ATRAC9 [Dragon Quest Builders (Vita), Final Fantaxy XV (PS4)] */
            atrac9_config cfg = {0};

            start_offset = sead.extradata_offset + sead.extradata_size;

            cfg.channels = vgmstream->channels;
            cfg.config_data = read_u32(sead.extradata_offset+0x0c, sf);
            cfg.encoder_delay = read_u32(sead.extradata_offset+0x18, sf);

            /* extradata: */
            /* 0x00: version */
            /* 0x01: reserved */
            /* 0x02: size */
            /* 0x04: block align */
            /* 0x06: block samples */
            /* 0x08: channel mask */
            /* 0x1c: config */
            /* 0x10: samples */
            /* 0x14: "overlap delay" */
            /* 0x18: "encoder delay" */
            /* 0x1c: sample rate */
            /* 0x24: loop start */
            /* 0x28: loop end */

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->channel_layout = read_u32(sead.extradata_offset+0x08,sf);
            vgmstream->sample_rate = read_u32(sead.extradata_offset+0x1c,sf); /* SAB's sample rate can be different/wrong */
            vgmstream->num_samples = read_u32(sead.extradata_offset+0x10,sf); /* loop values above are also weird and ignored */
            vgmstream->loop_start_sample = read_u32(sead.extradata_offset+0x20, sf) - (sead.loop_flag ? cfg.encoder_delay : 0); //loop_start
            vgmstream->loop_end_sample   = read_u32(sead.extradata_offset+0x24, sf) - (sead.loop_flag ? cfg.encoder_delay : 0); //loop_end
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x06: { /* MSMP3 (MSF subfile) [Dragon Quest Builders (PS3)] */
            mpeg_custom_config cfg = {0};

            start_offset = sead.extradata_offset + sead.extradata_size;

            /* extradata: */
            /* proper MSF header, but sample rate/loops are ignored in favor of SAB's */

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_bytes_to_samples(sead.stream_size, vgmstream->codec_data);
            vgmstream->loop_start_sample = sead.loop_start;
            vgmstream->loop_end_sample = sead.loop_end;
            break;
        }
#endif

        case 0x07: { /* HCA (subfile) [Dissidia Opera Omnia (Mobile), Final Fantaxy XV (PS4)] */
            VGMSTREAM* temp_vgmstream = NULL;
            STREAMFILE* temp_sf = NULL;
            off_t subfile_offset = sead.extradata_offset + 0x10;
            size_t subfile_size = sead.stream_size + sead.extradata_size - 0x10;

            size_t key_start = sead.extradata_id & 0xff;
            size_t header_size = read_u16(sead.extradata_offset+0x02, sf);
            int encryption = read_u8(sead.extradata_offset+0x0d, sf);
            /* encryption type 0x01 found in Final Fantasy XII TZA (PS4/PC)
             * (XOR subtable is fed to HCA's engine instead of generating from a key) */

            /* extradata (mostly same as HCA's): */
            /* 0x00: version */
            /* 0x01: size */
            /* 0x02: header size */
            /* 0x04: frame size */
            /* 0x06: loop start frame */
            /* 0x08: loop end frame */
            /* 0x0a: inserted samples (encoder delay) */
            /* 0x0c: "use mixer" flag */
            /* 0x0d: encryption flag */
            /* 0x0e: reserved x2 */
            /* 0x10+ HCA header */

            temp_sf = setup_sqex_streamfile(sf, subfile_offset, subfile_size, encryption, header_size, key_start, "hca");
            if (!temp_sf) goto fail;

            temp_vgmstream = init_vgmstream_hca(temp_sf);
            if (temp_vgmstream) {
                /* loops can be slightly different (~1000 samples) but probably HCA's are more accurate */
                temp_vgmstream->num_streams = vgmstream->num_streams;
                temp_vgmstream->stream_size = vgmstream->stream_size;
                temp_vgmstream->meta_type = vgmstream->meta_type;
                strcpy(temp_vgmstream->stream_name, vgmstream->stream_name);

                close_streamfile(temp_sf);
                close_vgmstream(vgmstream);
                return temp_vgmstream;
            }
            else {
                close_streamfile(temp_sf);
                goto fail;
            }
        }

        case 0x05: /* XMA2 (extradata may be a XMA2 fmt extra chunk) */
        case 0x08: /* SWITCHOPUS (no extradata?) */
        default:
            vgm_logi("SQEX SEAD: unknown codec %x\n", sead.codec);
            goto fail;
    }

    strcpy(vgmstream->stream_name, sead.readable_name);

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

//todo safeops, avoid recalc lens 
static void sead_cat(char* dst, int dst_max, const char* src) {
    int dst_len = strlen(dst);
    int src_len = strlen(dst);
    if (dst_len + src_len > dst_max - 1)
        return;
    strcat(dst, src);
}

static void build_readable_sab_name(sead_header_t* sead, STREAMFILE* sf, uint32_t sndname_offset, uint32_t sndname_size) {
    char * buf = sead->readable_name;
    int buf_size = sizeof(sead->readable_name);
    char descriptor[255], name[255];

    if (sead->filename_size > 255 || sndname_size > 255)
        goto fail;

    if (buf[0] == '\0') { /* init */
        read_string(descriptor,sead->filename_size+1, sead->filename_offset, sf);
        read_string(name, sndname_size+1, sndname_offset, sf);

        snprintf(buf,buf_size, "%s/%s", descriptor, name);
    }
    else { /* add */
        read_string(name, sndname_size+1, sndname_offset, sf);

        sead_cat(buf, buf_size, "; ");
        sead_cat(buf, buf_size, name);
    }
    return;
fail:
    VGM_LOG("SEAD: bad sab name found\n");
}

static void build_readable_mab_name(sead_header_t* sead, STREAMFILE* sf) {
    char * buf = sead->readable_name;
    int buf_size = sizeof(sead->readable_name);
    char descriptor[255], name[255], mode[255];

    if (sead->filename_size > 255 || sead->muscname_size > 255 || sead->sectname_size > 255 || sead->modename_size > 255)
        goto fail;

    read_string(descriptor,sead->filename_size+1,sead->filename_offset, sf);
    //read_string(filename,sead->muscname_size+1,sead->muscname_offset, sf); /* same as filename, not too interesting */
    if (sead->sectname_offset)
        read_string(name,sead->sectname_size+1,sead->sectname_offset, sf);
    else if (sead->instname_offset)
        read_string(name,sead->instname_size+1,sead->instname_offset, sf);
    else
        strcpy(name, "?");
    if (sead->modename_offset > 0)
        read_string(mode,sead->modename_size+1,sead->modename_offset, sf);

    /* default mode in most files */
    if (sead->modename_offset == 0 || strcmp(mode, "Mode") == 0 || strcmp(mode, "Mode0") == 0)
        snprintf(buf,buf_size, "%s/%s", descriptor, name);
    else
        snprintf(buf,buf_size, "%s/%s/%s", descriptor, name, mode);

    return;
fail:
    VGM_LOG("SEAD: bad mab name found\n");
}

static void parse_sead_mab_name(sead_header_t* sead, STREAMFILE* sf) {
    read_u32_t read_u32 = sead->big_endian ? read_u32be : read_u32le;
    read_u16_t read_u16 = sead->big_endian ? read_u16be : read_u16le;
    int i, j, k, entries;
    uint32_t target_muscname_offset = 0, target_sectname_offset = 0;
    uint32_t target_muscname_size = 0, target_sectname_size = 0;

    /* find names referencing to our material stream, usually:
     * - music > sections > layers (<> meters) > material index
     * - instruments > instrument materials > material index */


    /* parse musics */
    entries = read_u16(sead->musc_section_offset + 0x04, sf);
    for (i = 0; i < entries; i++) {
        off_t musc_offset = sead->musc_section_offset + read_u32(sead->musc_section_offset + 0x10 + i*0x04, sf);
        off_t muscname_offset, table_offset;
        size_t musc_size, muscname_size;
        int musc_version, sect_count, mode_count;

        musc_version = read_u8 (musc_offset + 0x00, sf);
        /* 0x01: "output"? */
        musc_size    = read_u16(musc_offset + 0x02, sf);
        sect_count   = read_u8 (musc_offset + 0x04, sf);
        mode_count   = read_u8 (musc_offset + 0x05, sf);
        /* 0x06: category? */
        /* 0x07: priority (default 0x80) */
        /* 0x08: number */
        /* 0x0a: flags */
        /* 0x0b: distance attenuation curve? */
        /* 0x0c: interior factor (f32) */
        /* 0x10: name in <=v8 */
        /* 0x20: audible range? (f32) */
        /* 0x24: inner range? (f32) */
        /* 0x28: volume (f32) */
        /* 0x2c: send busses x4 / send volumes (f32) x4 */
        /* 0x40: counts: aux sends/end methods/start methods/zero-ones */
        /* 0x44: sample rate */
        muscname_size  = read_u8(musc_offset + 0x48, sf);
        /* 0x49: port? */
        /* 0x4a: reserved */
        /* 0x4c: audio length (float) */
        /* 0x50+: extra data in later versions */

        if (musc_version <= 8) {
            muscname_offset = musc_offset + 0x10;
            muscname_size = 0x0f;
        }
        else {
            muscname_offset = musc_offset + musc_size;
        }

        table_offset = align_size_to_block(muscname_offset + muscname_size + 0x01, 0x10);

        /* parse sections (layered parts that possibly transition into others using "meter" info) */
        for (j = 0; j < sect_count; j++) {
            off_t sect_offset = musc_offset + read_u32(table_offset + j*0x04, sf);
            off_t sectname_offset, subtable_offset;
            size_t sect_size, sectname_size;
            int sect_version, layr_count;

            sect_version = read_u8 (sect_offset + 0x00, sf);
            /* 0x01: number */
            sect_size    = read_u16(sect_offset + 0x02, sf);

            if (sect_version <= 7) {
                /* 0x04: meter count */
                layr_count = read_u8(sect_offset + 0x05, sf);
                /* 0x06: custom points count */
                /* 0x08: entry points (sample) */
                /* 0x0c: exit points (sample) */
                /* 0x10: loop start */
                /* 0x14: loop end */
                /* 0x18+: meter transition timing info (offsets, points, curves, etc) */
                sectname_offset = 0x30;
                sectname_size = 0x0f;

                subtable_offset = sect_offset + sect_size;
            }
            else {
                sectname_size = read_u8(sect_offset + 0x04, sf);
                layr_count = read_u8(sect_offset + 0x05, sf);
                /* 0x06: custom points count */
                /* 0x08: entry point (sample) */
                /* 0x0c: exit point (sample) */
                /* 0x10: loop start */
                /* 0x14: loop end */
                /* 0x18: meter count */
                /* 0x1c+: meter transition timing info (offsets, points, curves, etc) */
                sectname_offset = sect_offset + sect_size;

                subtable_offset = align_size_to_block(sectname_offset + sectname_size + 0x01, 0x10);
            }


            if (j + 1== sead->target_subsong) {
                target_muscname_offset = muscname_offset;
                target_muscname_size = muscname_size;
                target_sectname_offset = sectname_offset;
                target_sectname_size = sectname_size;
            }

            /* parse layers */
            for (k = 0; k < layr_count; k++) {
                off_t layr_offset = sect_offset + read_u32(subtable_offset + k*0x04, sf);
                int mtrl_index;

                /* 0x00: version */
                /* 0x01: flags */
                /* 0x02: size */
                mtrl_index = read_u16(layr_offset + 0x04, sf);
                /* 0x06: loop count */
                /* 0x08: offset */
                /* 0x0c: end point (sample) */

                if (mtrl_index == sead->mtrl_index) {
                    sead->muscname_offset = muscname_offset;
                    sead->muscname_size   = muscname_size;
                    sead->sectname_offset = sectname_offset;
                    sead->sectname_size = sectname_size;
                    //break;
                }
            }

            /* use last name for cloned materials (see below) */
            //if (sead->sectname_offset > 0)
            //    break;

            /* meters offset go after layer offsets, not useful */
        }

        /* in some files (ex. FF12 TZA) materials are cloned, but sections only point to one of the
         * clones, so one material has multiple section names while others have none. For those cases
         * we can try to match names by subsong number */
        if (sead->sectname_offset == 0) {
            sead->muscname_offset = target_muscname_offset;
            sead->muscname_size   = target_muscname_size;
            sead->sectname_offset = target_sectname_offset;
            sead->sectname_size = target_sectname_size;
            VGM_LOG("MAB: voodoo name matching\n");
        }


        /* modes have names (almost always "Mode" and only 1 entry, rarely "Water" / "Restaurant" / etc)
         * and seem referenced manually (in-game events) and alter sound parameters instead of being
         * an actual playable thing (only found multiple in FFXV's bgm_gardina) */

        /* hack to use mode as subsong name, which for the only known file looks correct */
        if (mode_count == sead->total_subsongs)
            i = sead->target_subsong - 1;
        else
            i = 0;

        { //for (i = 0; i < mode_count; i++) {
            off_t mode_offset = musc_offset + read_u32(table_offset + sect_count*0x04 + i*0x04, sf);
            off_t modename_offset;
            size_t mode_size, modename_size;
            int mode_version;

            mode_version  = read_u8 (mode_offset + 0x00, sf);
            /* 0x01: flags */
            mode_size     = read_u16(mode_offset + 0x02, sf);
            /* 0x04: number */
            modename_size = read_u8 (mode_offset + 0x06, sf);
            /* 0x07: reserved */
            /* 0x08: transition param offset */
            /* 0x0c: reserved */
            /* 0x10: volume */
            /* 0x14: pitch */
            /* 0x18: lowpass */
            /* 0x1c: speed */
            /* 0x20: name <=v2 (otherwise null) */

            if (mode_version <= 2) {
                modename_offset = mode_offset + 0x20;
                modename_size = 0x0f;
            }
            else {
                modename_offset = mode_offset + mode_size;
            }

            sead->modename_offset = modename_offset;
            sead->modename_size = modename_size;
        }
    }


    /* parse instruments (very rare, ex. KH3 tut) */
    entries = read_u16(sead->inst_section_offset + 0x04, sf);
    for (i = 0; i < entries; i++) {
        off_t inst_offset = sead->inst_section_offset + read_u32(sead->inst_section_offset + 0x10 + i*0x04, sf);
        off_t instname_offset, mtrl_offset;
        size_t instname_size;
        int mtrl_count;

        /* 0x00: version */
        /* 0x01: "output"? */
        /* 0x02: size */
        /* 0x04: type (normal, random, switch, etc) */
        mtrl_count = read_u8(inst_offset + 0x05, sf);
        /* 0x06: category */
        /* 0x07: priority */
        /* 0x08: number */
        /* 0x0a: flags */
        /* 0x0b: distance attenuation curve */
        /* 0x0c: interior factor (f32) */
        /* 0x10: audible range (f32) */
        /* 0x14: inner range (f32) */
        /* 0x18: play length (f32) */
        /* 0x1c: reserved */
        /* 0x20: ? (f32) */
        /* 0x30: name */

        instname_offset = inst_offset + 0x30;
        instname_size = 0x0F;

        mtrl_offset = instname_offset + instname_size + 0x01;

        /* parse instrument materials */
        for (j = 0; j < mtrl_count; j++) {
            size_t mtrl_size;
            int mtrl_index;

            /* 0x00: version */
            /* 0x01: value (meaning depends on type) */
            mtrl_size  = read_u16(mtrl_offset + 0x02, sf);
            mtrl_index = read_u16(mtrl_offset + 0x04, sf);
            /* 0x06: number */
            /* 0x08: volume */
            /* 0x0c: sync point */
            /* 0x10: sample rate */
            /* 0x14-20: reserved */

            if (mtrl_index == sead->mtrl_index) {
                sead->instname_offset = instname_offset;
                sead->instname_size = instname_size;
                break;
            }

            mtrl_offset += mtrl_size;
        }

        if (sead->instname_offset > 0)
            break;
    }
}

static void parse_sead_sab_name(sead_header_t* sead, STREAMFILE* sf) {
    read_u32_t read_u32 = sead->big_endian ? read_u32be : read_u32le;
    read_u16_t read_u16 = sead->big_endian ? read_u16be : read_u16le;
    int i, j, entries;

    /* find names referencing to our material stream, usually:
     * - sound > sequence index
     * - sequence > command > track index
     * - track > material index
     *
     * most of the time sounds<>materials go 1:1 but there are exceptions
     * (ex. DQB se_break_soil, FFXV aircraftzeroone, FFXV 03bt100031pc00)
     *
     * some configs (mainly zero-ones, that seem to be a kind of random sound) have a name too,
     * but it's usually "Default" "(name)ZeroOne" and other uninteresting stuff */

    /* parse sounds */
    entries = read_u16(sead->snd_section_offset + 0x04, sf);
    for (i = 0; i < entries; i++) {
        uint32_t snd_offset = sead->snd_section_offset + read_u32(sead->snd_section_offset + 0x10 + i*0x04, sf);
        uint32_t snd_size, sndname_size;
        uint32_t seqi_start, seqi_offset, sndname_offset;
        int snd_version, seqi_count;

        snd_version = read_u8(snd_offset + 0x00, sf);
        /* 0x01: work? */
        snd_size = read_u16(snd_offset + 0x02, sf);
        /* 0x04: type */
        seqi_count = read_u8(snd_offset + 0x05, sf); /* may be 0 */
        /* 0x06: category */
        /* 0x07: priority (default 0x80) */
        /* 0x08: number */
        /* 0x0a: start+end macro */
        /* 0x0c: volume */
        /* 0x10: cycle config */
        /* 0x1a: header size? */
        seqi_start = read_u16(snd_offset + 0x1a, sf);
        /* 0x1c: audible range */
        /* 0x20: output/curve/port */
        /* 0x23: reserved/name size */
        /* 0x24: play length */
        /* 0x28+: more config params */

        if (snd_version <= 8) {
            sndname_size = 0x0F;
            sndname_offset = snd_offset + 0x50;
        }
        else {
            sndname_size = read_u8(snd_offset + 0x23, sf);
            sndname_offset = snd_offset + snd_size;
        }

        /* parse sequence info */
        seqi_offset = snd_offset + seqi_start;
        for (j = 0; j < seqi_count; j++) {
            size_t seqi_size;
            int seq_index;

            /* 0x00: version */
            /* 0x01: reserved */
            seqi_size = read_u16(seqi_offset + 0x02, sf);
            seq_index = read_u16(seqi_offset + 0x04, sf);
            /* 0x06: sequence ID */
            /* 0x08: reserved x2 */

            seqi_offset += seqi_size;

            /* parse sequence */
            {
                uint32_t seq_offset = sead->seq_section_offset + read_u32(sead->seq_section_offset + 0x10 + seq_index*0x04, sf);
                uint32_t cmnd_start, cmnd_offset;
                int seq_version;

                seq_version = read_u8(seq_offset + 0x00, sf);
                /* 0x01: reserved */
                /* 0x02: size */

                if (seq_version <= 2) {
                    /* 0x04: config depending on type */
                    /* 0x10: number */
                    /* 0x12: volume zero-one offsets */
                    /* 0x12: pitch zero-one offsets */
                    cmnd_start = read_u16(seq_offset + 0x16, sf);
                    /* 0x18: reserved x2 */
                }
                else {
                    /* 0x04: id */
                    cmnd_start = read_u16(seq_offset + 0x06, sf);
                    /* 0x08: zero-ones count */
                    /* 0x09: reserved */
                    /* 0x10: config depending on type */
                }


                /* parse sequence commands (breaks once an end command is found) */
                cmnd_offset = seq_offset + cmnd_start;
                while (cmnd_offset < sead->trk_section_offset) {
                    uint8_t cmnd_size, cmnd_type, cmnd_body;

                    /* 0x00: version (each command may have a different version) */
                    cmnd_size = read_u8(cmnd_offset + 0x01, sf); /* doesn't include body */
                    cmnd_type = read_u8(cmnd_offset + 0x02, sf);
                    cmnd_body = read_u8(cmnd_offset + 0x03, sf);

                    /* 0=none, 1=end, 2=key on, 3=interval, 4=keyoff, 5=wat, 6=loop start, 7=loop end, 8=trigger */
                    if (cmnd_type == 0x02) {
                        uint32_t trk_index;

                        trk_index = read_u32(cmnd_offset + cmnd_size + 0x00, sf);
                        /* 0x04: is loop */
                        /* 0x05: reserved */
                        /* 0x06: track id */
                        /* 0x08: play length (f32) */

                        /* parse track */
                        {
                            uint32_t trk_offset = sead->trk_section_offset + read_u32(sead->trk_section_offset + 0x10 + trk_index*0x04, sf);
                            uint8_t trk_type;
                            uint16_t mtrl_index;

                            /* 0x00: version */
                            trk_type = read_u8(trk_offset + 0x01, sf);
                            /* 0x02: size */
                            /* 0x04: config depending on type */
                            /* 0x08: id */
                            /* 0x0a: child id */
                            /* 0x0c: reserved */

                            /* 0=none, 1=material, 2=sound */
                            if (trk_type == 0x01) {
                                mtrl_index = read_u16(trk_offset + 0x04, sf);
                                /* 0x02: bank */

                                /* assumes same bank, not sure if bank info is even inside .sab */
                                if (mtrl_index == sead->mtrl_index) {
                                    build_readable_sab_name(sead, sf, sndname_offset, sndname_size);
                                    //sead->sndname_offset = sndname_offset;
                                    //sead->sndname_size = sndname_size;
                                }
                            }
                            else if (trk_type == 0x02) {
                                /* 0x00: index */
                                /* 0x02: reserved */
                                /* parse sound again? */
                            }
                        }
                    }

                    cmnd_offset += cmnd_size + cmnd_body;

                    /* commands normally end when a type 0=none is found */
                    if (cmnd_type <= 0x00 || cmnd_type > 0x08)
                        break;

                    /* keep reading names as N sounds may point to current material */
                    //if (sead->sndname_offset > 0)
                    //    break;
                }
            }

        }
    }
}


static int parse_sead(sead_header_t* sead, STREAMFILE* sf) {
    read_u32_t read_u32 = sead->big_endian ? read_u32be : read_u32le;
    read_u16_t read_u16 = sead->big_endian ? read_u16be : read_u16le;

    /** base header **/
    /* 0x00: id */
    /* 0x04: version (usually 0x02, rarely 0x01 ex FF XV early songs) */
    /* 0x05: flags */
    /* 0x06: size */
    /* 0x08: chunk count (in practice mab=3, sab=4) */
    sead->filename_size = read_u8(0x09, sf);
    /* 0x0a: number (shared between multiple sab/mab though) */
    if (read_u32(0x0c, sf) != get_streamfile_size(sf))
        goto fail;

    /* not set/reserved when version == 1 (name is part of size) */
    if (sead->filename_size == 0)
        sead->filename_size = 0x0f;
    sead->filename_offset = 0x10; /* file description ("BGM", "Music2", "SE", etc, long names are ok) */
    sead->sections_offset = sead->filename_offset + (sead->filename_size + 0x01); /* string null matters for padding */
    sead->sections_offset = align_size_to_block(sead->sections_offset, 0x10);

    /* roughly, mab play audio by calling musics/instruments, and sab with sounds/sequences/track. Both
     * point to a "material" or stream wave that may be reused. We only want material streams as subsongs,
     * but also try to find names from musics/sounds/etc. */

    /** chunk table elements (offsets to sections) **/
    /* 0x00: id */
    /* 0x04: version */
    /* 0x05: reserved */
    /* 0x06: size */
    /* 0x08: offset */
    /* 0x0c: reserved */
    if (sead->is_sab) {
        if (read_u32be(sead->sections_offset + 0x00, sf) != 0x736E6420) goto fail; /* "snd " (sounds) */
        if (read_u32be(sead->sections_offset + 0x10, sf) != 0x73657120) goto fail; /* "seq " (sequences) */
        if (read_u32be(sead->sections_offset + 0x20, sf) != 0x74726B20) goto fail; /* "trk " (tracks) */
        if (read_u32be(sead->sections_offset + 0x30, sf) != 0x6D74726C) goto fail; /* "mtrl" (material headers/streams) */
        sead->snd_section_offset  = read_u32(sead->sections_offset + 0x08, sf);
        sead->seq_section_offset  = read_u32(sead->sections_offset + 0x18, sf);
        sead->trk_section_offset  = read_u32(sead->sections_offset + 0x28, sf);
        sead->mtrl_section_offset = read_u32(sead->sections_offset + 0x38, sf);
    }
    else if (sead->is_mab) {
        if (read_u32be(sead->sections_offset + 0x00, sf) != 0x6D757363) goto fail; /* "musc" (musics) */
        if (read_u32be(sead->sections_offset + 0x10, sf) != 0x696E7374) goto fail; /* "inst" (instruments) */
        if (read_u32be(sead->sections_offset + 0x20, sf) != 0x6D74726C) goto fail; /* "mtrl" (material headers/streams) */
        sead->musc_section_offset = read_u32(sead->sections_offset + 0x08, sf);
        sead->inst_section_offset = read_u32(sead->sections_offset + 0x18, sf);
        sead->mtrl_section_offset = read_u32(sead->sections_offset + 0x28, sf);
    }
    else {
        goto fail;
    }


    /* section "chunk" format at offset: */
    /* 0x00: version */
    /* 0x01: reserved */
    /* 0x02: size */
    /* 0x04: entries */
    /* 0x06: reserved */
    /* 0x10+ 0x04*entry: offset to entry from table start */


    /* find target material offset */
    {
        int i, entries;

        entries = read_u16(sead->mtrl_section_offset + 0x04, sf);

        if (sead->target_subsong == 0) sead->target_subsong = 1;
        sead->total_subsongs = 0;
        sead->mtrl_offset = 0;

        /* manually find subsongs as entries can be dummy (ex. sfx banks in Dissidia Opera Omnia) */
        for (i = 0; i < entries; i++) {
            off_t entry_offset = sead->mtrl_section_offset + read_u32(sead->mtrl_section_offset + 0x10 + i*0x04, sf);

            //if (read_u8(entry_offset + 0x05, sf) == 0) {
            //    continue; /* codec 0 when dummy (see stream header) */
            //}

            sead->total_subsongs++;
            if (!sead->mtrl_offset && sead->total_subsongs == sead->target_subsong) {
                sead->mtrl_offset = entry_offset;
                sead->mtrl_index = i;
            }
        }

        /* SAB can contain 0 entries too */
        if (sead->mtrl_offset == 0) {
            vgm_logi("SQEX SEAD: bank has no subsongs (ignore)\n");
            goto fail;
        }
    }

    /** stream header **/
    /* 0x00: version */
    /* 0x01: reserved */
    /* 0x02: size */
    sead->channels      = read_u8 (sead->mtrl_offset + 0x04, sf);
    sead->codec         = read_u8 (sead->mtrl_offset + 0x05, sf); /* format */
    sead->mtrl_number   = read_u16(sead->mtrl_offset + 0x06, sf); /* 0..N */
    sead->sample_rate   = read_u32(sead->mtrl_offset + 0x08, sf);
    sead->loop_start    = read_u32(sead->mtrl_offset + 0x0c, sf); /* in samples but usually ignored */
    sead->loop_end      = read_u32(sead->mtrl_offset + 0x10, sf);
    sead->extradata_size= read_u32(sead->mtrl_offset + 0x14, sf); /* including subfile header, can be 0 */
    sead->stream_size   = read_u32(sead->mtrl_offset + 0x18, sf); /* not including subfile header */
    sead->extradata_id  = read_u16(sead->mtrl_offset + 0x1c, sf);
    /* 0x1e: reserved */

    if (sead->codec == 0x00) {
        /* dummy entries have null fields, put defaults to allow playing them */
        sead->channels = 1;
        sead->sample_rate = 48000;
    }

    sead->loop_flag       = (sead->loop_end > 0);
    sead->extradata_offset = sead->mtrl_offset + 0x20;

    if (sead->is_sab) {
        parse_sead_sab_name(sead, sf);
        /* sab name is added during process */
    }
    else if (sead->is_mab) {
        parse_sead_mab_name(sead, sf);
        build_readable_mab_name(sead, sf);
    }


    return 1;
fail:
    return 0;
}
