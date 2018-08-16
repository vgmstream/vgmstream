#include "meta.h"
#include "../coding/coding.h"


typedef enum { UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_DSP, RAW_XBOX, FMT_VAG, FMT_AT3, FMT_OGG } ubi_sb_codec;
typedef struct {
    int big_endian;
    int total_streams;
    int is_external;
    int autodetect_external;
    ubi_sb_codec codec;

    /* main/fixed info */
    uint32_t version;
    size_t section1_num;
    size_t section2_num;
    size_t section3_num;
    size_t extra_size;
    int flag1;
    int flag2;

    /* stream info config (format varies slightly per game) */
    size_t section1_entry_size;
    size_t section2_entry_size;
    size_t section3_entry_size;
    off_t external_flag_offset;
    off_t num_samples_offset;
    off_t sample_rate_offset;
    off_t channels_offset;
    off_t stream_type_offset;
    off_t stream_name_offset;
    off_t extra_name_offset;
    size_t stream_name_size;
    size_t stream_id_offset;
    int has_short_channels;
    int has_internal_names;
    int has_extra_name_flag;
    int has_rotating_ids;

    /* derived */
    size_t main_size;
    size_t section1_size;
    size_t section2_size;
    size_t section3_size;

    /* stream info */
    uint32_t header_id;
    uint32_t header_type;
    size_t stream_size;
    off_t extra_offset;
    off_t stream_offset;
    uint32_t stream_id;

    int stream_samples; /* usually only for external resources */
    int sample_rate;
    int channels;
    uint32_t stream_type;
    char stream_name[255];


} ubi_sb_header;

static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile);
static int config_sb_header_version(ubi_sb_header * sb, STREAMFILE *streamFile);


/* .SBx - banks from Ubisoft's sound engine ("DARE" / "UbiSound Driver") games in ~2000-2008 */
VGMSTREAM * init_vgmstream_ubi_sb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *streamData = NULL;
    off_t start_offset;
    int loop_flag = 0;
    ubi_sb_header sb = {0};

    /* check extension (number represents the platform, see later) */
    if ( !check_extensions(streamFile,"sb0,sb1,sb2,sb3,sb4,sb5,sb6,sb7") )
        goto fail;


    /* .sb0 (sound bank) is a small multisong format (loaded in memory?) that contains SFX data
     * but can also reference .ss0/ls0 (sound stream) external files for longer streams.
     * A companion .sp0 (sound project) describes files and if it uses BANKs (.sb0) or MAPs (.sm0). */

    /* main parse */
    if ( !parse_sb_header(&sb, streamFile) )
        goto fail;


    /* open external stream if needed */
    if (sb.autodetect_external) { /* works most of the time but could give false positives */
        VGM_LOG("UBI SB: autodetecting external stream '%s'\n", sb.stream_name);

        streamData = open_streamfile_by_filename(streamFile,sb.stream_name);
        if (!streamData) {
            streamData = streamFile; /* assume internal */
            if (sb.stream_size > get_streamfile_size(streamData)) {
                VGM_LOG("UBI SB: expected external stream\n");
                goto fail;
            }
        } else {
            sb.is_external = 1;
        }
    }
    else if (sb.is_external) {
        streamData = open_streamfile_by_filename(streamFile,sb.stream_name);
        if (!streamData) {
            VGM_LOG("UBI SB: external stream '%s' not found\n", sb.stream_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }

    /* final offset */
    if (sb.is_external) {
        start_offset = sb.stream_offset;
    } else {
        start_offset  = sb.main_size + sb.section1_size + sb.section2_size + sb.extra_size + sb.section3_size;
        start_offset += sb.stream_offset;
    }
    //;VGM_LOG("start offset=%lx, external=%i\n", start_offset, sb.is_external);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sb.channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sb.sample_rate;
    vgmstream->num_streams = sb.total_streams;
    vgmstream->stream_size = sb.stream_size;
    vgmstream->meta_type = meta_UBI_SB;


    switch(sb.codec) {
        case UBI_ADPCM: {
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = ubi_ima_bytes_to_samples(sb.stream_size, sb.channels, streamData, start_offset);
            break;
        }

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE even on Wii */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(sb.stream_size, sb.channels, 16);
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb.stream_size / sb.channels;
            vgmstream->num_samples = ps_bytes_to_samples(sb.stream_size, sb.channels) ;
            break;

        case RAW_XBOX:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(sb.stream_size, sb.channels);
            break;

        case RAW_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb.stream_size / sb.channels;
            vgmstream->num_samples = dsp_bytes_to_samples(sb.stream_size, sb.channels);

            {
                off_t coefs_offset = sb.main_size + sb.section1_size + sb.section2_size + sb.extra_offset;
                coefs_offset += 0x10; /* entry size is 0x40 (first/last 0x10 = unknown), per channel */

                dsp_read_coefs_be(vgmstream,streamFile,coefs_offset, 0x40);
            }
            break;

        case FMT_VAG:
            /* skip VAG header (some sb4 use VAG and others raw PSX) */
            if (read_32bitBE(start_offset, streamData) == 0x56414770) { /* "VAGp" */
                start_offset += 0x30;
                sb.stream_size  -= 0x30;
            }

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb.stream_size / sb.channels;
            vgmstream->num_samples = ps_bytes_to_samples(sb.stream_size, sb.channels);

            break;

#ifdef VGM_USE_FFMPEG
        case FMT_AT3: {
            ffmpeg_codec_data *ffmpeg_data;

            /* skip weird value (3, 4) in Brothers in Arms: D-Day (PSP) */
            if (read_32bitBE(start_offset+0x04,streamData) == 0x52494646) {
                start_offset += 0x04;
                sb.stream_size -= 0x04;
            }

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (sb.stream_samples == 0) /* sometimes not known */
                sb.stream_samples = ffmpeg_data->totalSamples;
            vgmstream->num_samples = sb.stream_samples;
            if (sb.stream_samples != ffmpeg_data->totalSamples) {
                VGM_LOG("UBI SB: header samples differ (%i vs %i)\n", sb.stream_samples, (size_t)ffmpeg_data->totalSamples);
                goto fail;
            }

            if (ffmpeg_data->skipSamples <= 0) /* in case FFmpeg didn't get them */
                ffmpeg_set_skip_samples(ffmpeg_data, riff_get_fact_skip_samples(streamData, start_offset));
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = sb.stream_samples; /* ffmpeg_data->totalSamples */
            VGM_ASSERT(sb.stream_samples != ffmpeg_data->totalSamples, "UBI SB: header samples differ\n");
            break;
        }

#endif
        default:
            VGM_LOG("UBI SB: unknown codec\n");
            goto fail;
    }

    if (sb.has_internal_names || sb.is_external) {
        strcpy(vgmstream->stream_name, sb.stream_name);
    }


    /* open the file for reading (can be an external stream, different from the current .sb0) */
    if ( !vgmstream_open_stream(vgmstream, streamData, start_offset) )
        goto fail;

    if (sb.is_external && streamData) close_streamfile(streamData);
    return vgmstream;

fail:
    if (sb.is_external && streamData) close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;

}


static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int i, ok, current_type = -1, current_id = -1;
    int target_stream = streamFile->stream_index;

    if (target_stream == 0) target_stream = 1;

    sb->big_endian = check_extensions(streamFile, "sb3,sb6,sb7"); /* GC, PS3, Wii */
    if (sb->big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    /* file layout is: base header, section1, section2, extra section, section3, data (all except base header can be null) */

    sb->version      = read_32bit(0x00, streamFile); /* 16b+16b major/minor version */
    sb->section1_num = read_32bit(0x04, streamFile); /* group headers? */
    sb->section2_num = read_32bit(0x08, streamFile); /* streams headers (internal or external) */
    sb->section3_num = read_32bit(0x0c, streamFile); /* internal streams table */
    sb->extra_size   = read_32bit(0x10, streamFile); /* extra table, unknown (config for non-audio types) except with DSP = coefs */
    sb->flag1        = read_32bit(0x14, streamFile); /* unknown, usually -1 but can be others (0/1/2/etc) */
    sb->flag2        = read_32bit(0x18, streamFile); /* unknown, usually -1 but can be others  */

    ok = config_sb_header_version(sb, streamFile);
    if (!ok) {
        VGM_LOG("UBI SB: unknown SB version+platform\n");
        goto fail;
    }

    sb->main_size     = 0x1c;
    sb->section1_size = sb->section1_entry_size * sb->section1_num;
    sb->section2_size = sb->section2_entry_size * sb->section2_num;
    sb->section3_size = sb->section3_entry_size * sb->section3_num;

    /* find target stream info in section2 */
    for (i = 0; i < sb->section2_num; i++) {
        off_t offset = sb->main_size + sb->section1_size + sb->section2_entry_size*i;

        /* ignore non-audio entry (other types seem to have config data) */
        if (read_32bit(offset + 0x04, streamFile) != 0x01)
            continue;
        //;VGM_LOG("SB at %lx\n", offset);

        /* weird case when there is no internal substream ID and just seem to rotate every time type changes, joy */
        if (sb->has_rotating_ids) { /* assumes certain configs can't happen in this case */
            int current_is_external = 0;
            int type = read_32bit(offset + sb->stream_type_offset, streamFile);

            if (current_type == -1)
                current_type = type;
            if (current_id == -1) /* use first ID in section3 */
                current_id = read_32bit(sb->main_size + sb->section1_size + sb->section2_size + sb->extra_size + 0x00, streamFile);

            if (sb->external_flag_offset) {
                current_is_external = read_32bit(offset + sb->external_flag_offset, streamFile);
            } else if (sb->has_extra_name_flag && read_32bit(offset + sb->extra_name_offset, streamFile) != 0xFFFFFFFF) {
                current_is_external = 1; /* -1 in extra_name means internal */
            }


            if (!current_is_external) {
                if (current_type != type) {
                    current_type = type;
                    current_id++; /* rotate */
                    if (current_id >= sb->section3_num)
                        current_id = 0; /* reset */
                }

            }
        }

        /* update streams (total_stream also doubles as current) */
        sb->total_streams++;
        if (sb->total_streams != target_stream)
            continue;
        //;VGM_LOG("target at offset=%lx (size=%x)\n", offset, sb->section2_entry_size);

        sb->header_id      = read_32bit(offset + 0x00, streamFile); /* 16b+16b group+sound id */
        sb->header_type    = read_32bit(offset + 0x04, streamFile);
        sb->stream_size    = read_32bit(offset + 0x08, streamFile);
        sb->extra_offset   = read_32bit(offset + 0x0c, streamFile); /* within the extra section */
        sb->stream_offset  = read_32bit(offset + 0x10, streamFile); /* within the data section */
        sb->channels       = (sb->has_short_channels) ?
                   (uint16_t)read_16bit(offset + sb->channels_offset, streamFile) :
                   (uint32_t)read_32bit(offset + sb->channels_offset, streamFile);
        sb->sample_rate    = read_32bit(offset + sb->sample_rate_offset, streamFile);
        sb->stream_type    = read_32bit(offset + sb->stream_type_offset, streamFile);

        if (sb->num_samples_offset)
            sb->stream_samples = read_32bit(offset + sb->num_samples_offset, streamFile);

        if (sb-> has_rotating_ids) {
            sb->stream_id  = current_id;
        } else if (sb->stream_id_offset) {
            sb->stream_id  = read_32bit(offset + sb->stream_id_offset, streamFile);
        }

        /* external stream name can be found in the header (first versions) or the extra table (later versions) */
        if (sb->stream_name_offset) {
            read_string(sb->stream_name, sb->stream_name_size, offset + sb->stream_name_offset, streamFile);
        } else {
            sb->stream_name_offset = read_32bit(offset + sb->extra_name_offset, streamFile);
            read_string(sb->stream_name, sb->stream_name_size, sb->main_size + sb->section1_size + sb->section2_size + sb->stream_name_offset, streamFile);
        }

        /* not always set and must be derived */
        if (sb->external_flag_offset) {
            sb->is_external = read_32bit(offset + sb->external_flag_offset, streamFile);
        } else if (sb->has_extra_name_flag && read_32bit(offset + sb->extra_name_offset, streamFile) != 0xFFFFFFFF) {
            sb->is_external = 1; /* -1 in extra_name means internal */
        } else if (sb->section3_num == 0) {
            sb->is_external = 1;
        } else {
            sb->autodetect_external = 1;

            if (sb->stream_name[0] == '\0')
                sb->autodetect_external = 0; /* no name */
            if (sb->extra_size > 0 && sb->stream_name_offset > sb->extra_size)
                sb->autodetect_external = 0; /* name outside extra table == is internal */
        }
    }
    if (sb->total_streams == 0) {
        VGM_LOG("UBI SB: no streams\n");
        goto fail;
    }
    if (target_stream < 0 || target_stream > sb->total_streams || sb->total_streams < 1) {
        VGM_LOG("UBI SB: wrong target stream (target=%i, total=%i)\n", target_stream, sb->total_streams);
        goto fail;
    }

    if (!(sb->stream_id_offset || sb->has_rotating_ids) && sb->section3_num > 1) {
        VGM_LOG("UBI SB: unexpected number of internal streams %i\n", sb->section3_num);
        goto fail;
    }


    /* happens in some versions */
    if (sb->stream_type > 0xFF) {
        VGM_LOG("UBI SB: garbage in stream_type\n");
        sb->stream_type = 0;
    }

    /* guess codec */
    switch(sb->stream_type) {
        case 0x00: /* platform default (rarely external) */
            if (       check_extensions(streamFile, "sb0")) {
                sb->codec = RAW_PCM;
            } else if (check_extensions(streamFile, "sb1,sb5")) {
                sb->codec = RAW_PSX;
            } else if (check_extensions(streamFile, "sb2")) {
                sb->codec = RAW_XBOX;
            } else if (check_extensions(streamFile, "sb3,sb7")) {
                sb->codec = RAW_DSP;
            } else if (check_extensions(streamFile, "sb4")) {
                sb->codec = FMT_VAG;
            } else {
                VGM_LOG("UBI SB: unknown internal format\n");
                goto fail;
            }
            break;

        case 0x01: /* PCM (Wii, rarely used) */
            sb->codec = RAW_PCM;
            break;

        case 0x03: /* Ubi ADPCM (main external stream codec, has subtypes) */
            sb->codec = UBI_ADPCM;
            break;

        case 0x04: /* Ogg (later PC games) */
            sb->codec = FMT_OGG;
            break;

        case 0x05: /* AT3 (PSP, PS3) */
            sb->codec = FMT_AT3;
            break;

        default:
            VGM_LOG("UBI SB: unknown stream_type %x\n", sb->stream_type);
            goto fail;
    }

    /* uncommon but possible */
    //VGM_ASSERT(sb->is_external && sb->section3_num != 0, "UBI SS: mixed external and internal streams\n");

    /* seems that can be safely ignored */
    //VGM_ASSERT(sb->is_external && sb->stream_id_offset && sb->stream_id > 0, "UBI SB: unexpected external stream with stream id\n");

    /* section 3: substreams within the file, adjust stream offset (rarely used but table is always present) */
    if (!sb->is_external && (sb->stream_id_offset || sb->has_rotating_ids) && sb->section3_num > 1) {
        for (i = 0; i < sb->section3_num; i++) {
            off_t offset = sb->main_size + sb->section1_size + sb->section2_size + sb->extra_size + sb->section3_entry_size * i;

            /* table has unordered ids+size, so if our id doesn't match current data offset must be beyond */
            if (read_32bit(offset + 0x00, streamFile) == sb->stream_id)
                break;
            sb->stream_offset += read_32bit(offset + 0x04, streamFile);
        }
    }

    return 1;
fail:
    return 0;
}


static int config_sb_header_version(ubi_sb_header * sb, STREAMFILE *streamFile) {
    /* meh... */
    int is_sb0 = check_extensions(streamFile, "sb0"); /* PC */
    int is_sb1 = check_extensions(streamFile, "sb1"); /* PS2 */
    int is_sb2 = check_extensions(streamFile, "sb2"); /* Xbox */
    int is_sb3 = check_extensions(streamFile, "sb3"); /* GC */
    int is_sb4 = check_extensions(streamFile, "sb4"); /* PSP, X360? */
    int is_sb5 = check_extensions(streamFile, "sb5"); /* PSP, 3DS? */
    //int is_sb6 = check_extensions(streamFile, "sb6"); /* PS3? */
    int is_sb7 = check_extensions(streamFile, "sb7"); /* Wii */
    int is_biadd_psp = 0;


    /* The format varies with almost every game + platform (some kind of class serialization?),
     * support is done case-by-case as offsets move slightly. Basic layout:
     * - fixed part (0x00..0x14)
     * - garbage, flags
     * - external stream number of samples / internal garbage
     * - external stream size (also in the common part) / internal garbage
     * - garbage
     * - external original sample rate / internal garbage
     * - sample rate, pcm bits?, channels
     * - stream type and external filename (internal filename too on some platforms)
     * - end flags/garbage
     * Garbage looks like uninitialized variables (may be null, contain part of strings, etc).
     * Later version don't have filename per header but in the extra section.
     */


    /* common */
    sb->section3_entry_size = 0x08;
    sb->stream_name_size = 0x24; /* maybe 0x28 or 0x20 for some but ok enough (null terminated) */

    /* Prince of Persia: Sands of Time (2003)(PC) */
    if ((sb->version == 0x000A0002 && is_sb0) || /* (not sure if exists, just in case) */
        (sb->version == 0x000A0004 && is_sb0)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x80;

        sb->external_flag_offset = 0x24; /* maybe 0x28 */
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4a;
        sb->stream_type_offset   = 0x4c;
        sb->stream_name_offset   = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(PS2) */
    if ((sb->version == 0x000A0002 && is_sb1) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && is_sb1)) { /* main game */
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0; /* no apparent flag */
        sb->channels_offset      = 0x20;
        sb->sample_rate_offset   = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->stream_name_offset   = 0x40;
        sb->stream_type_offset   = 0x68;

        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(Xbox) */
    if ((sb->version == 0x000A0002 && is_sb2) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && is_sb2)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x78;

        sb->external_flag_offset = 0x24; /* maybe 0x28 */
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4a;
        sb->stream_type_offset   = 0x4c; /* may contain garbage */
        sb->stream_name_offset   = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        return 1;
    }

    /* Tom Clancy's Rainbow Six 3 (2003)(PS2) */
    if (sb->version == 0x000A0007 && is_sb1) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0; /* no apparent flag */
        sb->channels_offset      = 0x20;
        sb->sample_rate_offset   = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->stream_name_offset   = 0x40;
        sb->stream_type_offset   = 0x68;

        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(GC) */
    if ((sb->version == 0x000A0002 && is_sb3) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && is_sb3)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x74;

        sb->external_flag_offset = 0x20; /* maybe 0x24 */
        sb->num_samples_offset   = 0x2c;
        sb->sample_rate_offset   = 0x40;
        sb->channels_offset      = 0x46;
        sb->stream_type_offset   = 0x48;
        sb->stream_name_offset   = 0x4c;

        sb->has_short_channels = 1;
        return 1;
    }

    /* Myst IV Demo (2004)(PC) (final game is different) */
    if (sb->version == 0x00100000 && is_sb0) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0xa4;

        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0x34;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->stream_name_offset   = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(PC) */
    if (sb->version == 0x00120009 && is_sb0) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x84;

        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->stream_name_offset   = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(PS2) */
    if (sb->version == 0x00120009 && is_sb1) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0; /* no apparent flag */
        sb->channels_offset      = 0x20;
        sb->sample_rate_offset   = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->stream_name_offset   = 0x40;
        sb->stream_type_offset   = 0x68;

        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(Xbox) */
    if (sb->version == 0x00120009 && is_sb2) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x90;

        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0x44;
        sb->sample_rate_offset   = 0x58;
        sb->channels_offset      = 0x60;
        sb->stream_type_offset   = 0x64; /* may contain garbage */
        sb->stream_name_offset   = 0x68;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(GC) */
    if (sb->version == 0x00120009 && is_sb3) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x78;

        sb->external_flag_offset = 0x20;
        sb->num_samples_offset   = 0x2c;
        sb->sample_rate_offset   = 0x40;
        sb->channels_offset      = 0x48;
        sb->stream_type_offset   = 0x4c;
        sb->stream_name_offset   = 0x50;

        return 1;
    }

    /* two games with same id; use project file as identifier */
    if (sb->version == 0x0012000C && is_sb4) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "BIAAUDIO.SP4");
        if (streamTest) {
            is_biadd_psp = 1;
            close_streamfile(streamTest);
        }
    }

    /* Prince of Persia: Revelations (2005)(PSP) */
    if (sb->version == 0x0012000C && is_sb4 && !is_biadd_psp) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x84;

        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->stream_name_offset   = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Brothers in Arms - D-Day (2006)(PSP) */
    if (sb->version == 0x0012000C && is_sb4 && is_biadd_psp) {
        sb->section1_entry_size = 0x80;
        sb->section2_entry_size = 0x94;

        sb->stream_id_offset     = 0x0; //todo 0x1C or 0x20? table seems problematic
        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0; /* variable? */
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->stream_name_offset   = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PC) */
    if (sb->version == 0x00150000 && is_sb0) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x78;

        sb->external_flag_offset = 0x2c;
        sb->stream_id_offset     = 0x34;
        sb->num_samples_offset   = 0x40;
        sb->sample_rate_offset   = 0x54;
        sb->channels_offset      = 0x5c;
        sb->stream_type_offset   = 0x60;
        sb->extra_name_offset    = 0x64;

        sb->has_extra_name_flag = 1;
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PS2) */
    if (sb->version == 0x00150000 && is_sb1) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x5c;

        sb->external_flag_offset = 0;
        sb->channels_offset      = 0x2c;
        sb->sample_rate_offset   = 0x30;
        sb->num_samples_offset   = 0x3c;
        sb->extra_name_offset    = 0x4c;
        sb->stream_type_offset   = 0x50;

        sb->has_extra_name_flag = 1;
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(Xbox) */
    if (sb->version == 0x00150000 && is_sb2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->external_flag_offset = 0;
        sb->num_samples_offset   = 0x28;
        sb->stream_id_offset     = 0;
        sb->sample_rate_offset   = 0x3c;
        sb->channels_offset      = 0x44;
        sb->stream_type_offset   = 0x48;
        sb->extra_name_offset    = 0x4c;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1;
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(GC) */
    if (sb->version == 0x00150000 && is_sb3) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0x28; /* maybe 0x2c */
        sb->num_samples_offset   = 0x3c;
        sb->sample_rate_offset   = 0x50;
        sb->channels_offset      = 0x58;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x60;

        return 1;
    }
#if 0
    /* Far cry: Instincts - Evolution (2006)(Xbox) */
    if (sb->version == 0x00170000 && is_sb2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0;
        sb->num_samples_offset   = 0x28;
        sb->stream_id_offset     = 0;
        sb->sample_rate_offset   = 0x3c;
        sb->channels_offset      = 0x44;
        sb->stream_type_offset   = 0x48;
        sb->extra_name_offset    = 0x58;

        return 1;
    }
#endif

    /* Prince of Persia: Rival Swords (2007)(PSP) */
    if (sb->version == 0x00180005 && is_sb5) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->external_flag_offset = 0;
        sb->channels_offset      = 0x28;
        sb->sample_rate_offset   = 0x2c;
        //sb->num_samples_offset = 0x34 or 0x3c /* varies */
        sb->extra_name_offset    = 0x44;
        sb->stream_type_offset   = 0x48;

        sb->has_extra_name_flag = 1;
        return 1;
    }

#if 0
    /* Rainbow Six Vegas (2007)(PSP) */
    if (sb->version == 0x00180006 && is_sb5) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->external_flag_offset = 0;
        sb->channels_offset      = 0x28;
        sb->sample_rate_offset   = 0x2c;
        //sb->num_samples_offset = 0x34 or 0x3c /* varies */
        sb->extra_name_offset    = 0x44;
        sb->stream_type_offset   = 0x48;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1;
        return 1;
    }

    /* todo Rainbow Six Vegas changes:
     * some streams use type 0x06 instead of 0x01, known values:
     *   0x0c: header offset in extra table?
     *   0x2c: stream size
     *   0x30: stream offset
     *   most other fields are fixed, comparing different files
     * header is in the extra table, after the stream name (repeated?)
     * (0x04: sample rate, 0x0c: channels, etc)
     * stream data may be newer Ubi ADPCM (see DecUbiSnd)
     */
#endif

    /* Red Steel (2006)(Wii) */
    if (sb->version == 0x00180006 && is_sb7) { /* same as 0x00150000 */
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0x28; /* maybe 0x2c */
        sb->num_samples_offset   = 0x3c;
        sb->sample_rate_offset   = 0x50;
        sb->channels_offset      = 0x58;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x60;

        return 1;
    }

    /* Prince of Persia: Rival Swords (2007)(Wii) */
    if (sb->version == 0x00190003 && is_sb7) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->external_flag_offset = 0x28; /* maybe 0x2c */
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->extra_name_offset    = 0x58;
        sb->stream_type_offset   = 0x5c;

        return 1;
    }

    /* TMNT (2007)(PS2) */
    if (sb->version == 0x00190002 && is_sb1) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x5c;

        sb->external_flag_offset = 0;
        sb->channels_offset      = 0x28;
        sb->sample_rate_offset   = 0x2c;
        //sb->num_samples_offset = 0x34 or 0x3c /* varies */
        sb->extra_name_offset    = 0x44;
        sb->stream_type_offset   = 0x48;

        sb->has_extra_name_flag = 1;
        return 1;
    }

    /* TMNT (2007)(GC) */
    if (sb->version == 0x00190002 && is_sb3) { /* same as 0x00190003 */
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->external_flag_offset = 0x28; /* maybe 0x2c */
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;

        return 1;
    }

    /* Surf's Up (2007)(PC) */
    if (sb->version == 0x00190005 && is_sb0) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->external_flag_offset = 0x28; /* maybe 0x2c */
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;

        sb->has_extra_name_flag = 1;
        return 1;
    }

    return 0;
}
