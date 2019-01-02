#include "meta.h"
#include "../coding/coding.h"


typedef enum { UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_DSP, RAW_XBOX, FMT_VAG, FMT_AT3, RAW_AT3, FMT_XMA1, RAW_XMA1, FMT_OGG } ubi_sb_codec;
typedef enum { UBI_PC, UBI_PS2, UBI_XBOX, UBI_GC, UBI_X360, UBI_3DS, UBI_PS3, UBI_WII, UBI_PSP } ubi_sb_platform;
typedef struct {
    ubi_sb_platform platform;
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
    size_t section4_num;
    size_t extra_size;
    int flag1;
    int flag2;

    /* maps data config */
    int is_map;
    int map_version;

    /* stream info config (format varies slightly per game) */
    size_t section1_entry_size;
    size_t section2_entry_size;
    size_t section3_entry_size;
    off_t stream_size_offset;
    off_t extra_pointer_offset;
    off_t stream_pointer_offset;
    off_t external_flag_offset;
    off_t samples_flag_offset;
    off_t num_samples_offset;
    off_t num_samples_offset2;
    off_t sample_rate_offset;
    off_t channels_offset;
    off_t stream_type_offset;
    off_t stream_name_offset;
    off_t extra_name_offset;
    size_t stream_name_size;
    off_t stream_id_offset;
    off_t xma_pointer_offset;
    int has_short_channels;
    int has_internal_names;
    int has_extra_name_flag;
    int has_rotating_ids;

    /* derived */
    size_t section1_offset;
    size_t section2_offset;
    size_t section3_offset;
    size_t section4_offset;
    size_t extra_section_offset;
    size_t sounds_offset;

    /* map info */
    off_t map_header_offset;
    off_t map_num;
    off_t map_offset;
    char map_name[255];

    /* stream info */
    uint32_t header_id;
    uint32_t header_type;
    size_t stream_size;
    off_t extra_offset;
    off_t stream_offset;
    uint32_t stream_id;
    off_t xma_header_offset;

    int stream_samples; /* usually only for external resources */
    int sample_rate;
    int channels;
    uint32_t stream_type;
    char stream_name[255];
    char extra_name[255];
    int header_idx;
    off_t header_offset;

    int subtypes[16];
} ubi_sb_header;

static VGMSTREAM * init_vgmstream_ubi_sb_main(ubi_sb_header *sb, STREAMFILE *streamFile);
static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile, int target_stream);
static int config_sb_header_version(ubi_sb_header * sb, STREAMFILE *streamFile);

/* .SBx - banks from Ubisoft's sound engine ("DARE" / "UbiSound Driver") games in ~2000-2008 */
VGMSTREAM * init_vgmstream_ubi_sb(STREAMFILE *streamFile) {
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    //int16_t(*read_16bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = { 0 };
    int ok;
    int target_stream = streamFile->stream_index;

    /* check extension (number represents the platform, see later) */
    if (!check_extensions(streamFile, "sb0,sb1,sb2,sb3,sb4,sb5,sb6,sb7"))
        goto fail;

    if (target_stream == 0) target_stream = 1;

    /* .sb0 (sound bank) is a small multisong format (loaded in memory?) that contains SFX data
     * but can also reference .ss0/ls0 (sound stream) external files for longer streams.
     * A companion .sp0 (sound project) describes files and if it uses BANKs (.sb0) or MAPs (.sm0). */

    /* sigh... PSP hijacks not one but *two* platform indexes */
    /* please add any PSP game versions under sb4 and sb5 sections so we can properly identify platform */
    sb.version = read_32bitLE(0x00, streamFile);

    if (check_extensions(streamFile, "sb0")) {
        sb.platform = UBI_PC;
    } else if (check_extensions(streamFile, "sb1")) {
        sb.platform = UBI_PS2;
    } else if (check_extensions(streamFile, "sb2")) {
        sb.platform = UBI_XBOX;
    } else if (check_extensions(streamFile, "sb3")) {
        sb.platform = UBI_GC;
    } else if (check_extensions(streamFile, "sb4")) {
        switch (sb.version) {
            case 0x0012000C: /* Prince of Persia: Revelations (2005) */
                sb.platform = UBI_PSP;
                break;
            default:
                sb.platform = UBI_X360;
                break;
        }
    } else if (check_extensions(streamFile, "sb5")) {
        switch (sb.version) {
            case 0x00180005: /* Prince of Persia: Rival Swords (2007) */
            case 0x00180006: /* Rainbow Six Vegas (2007) */
                sb.platform = UBI_PSP;
                break;
            default:
                sb.platform = UBI_3DS;
                break;
        }
    } else if (check_extensions(streamFile, "sb6")) {
        sb.platform = UBI_PS3;
    } else if (check_extensions(streamFile, "sb7")) {
        sb.platform = UBI_WII;
    } else {
        goto fail;
    }

    sb.big_endian = (sb.platform == UBI_GC ||
        sb.platform == UBI_PS3 ||
        sb.platform == UBI_X360 ||
        sb.platform == UBI_WII);
    if (sb.big_endian) {
        read_32bit = read_32bitBE;
        //read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        //read_16bit = read_16bitLE;
    }

    /* file layout is: base header, section1, section2, extra section, section3, data (all except base header can be null) */

    sb.version = read_32bit(0x00, streamFile); /* 16b+16b major/minor version */
    sb.section1_num = read_32bit(0x04, streamFile); /* group headers? */
    sb.section2_num = read_32bit(0x08, streamFile); /* streams headers (internal or external) */
    sb.section3_num = read_32bit(0x0c, streamFile); /* internal streams table */
    sb.extra_size = read_32bit(0x10, streamFile); /* extra table, unknown (config for non-audio types) except with DSP = coefs */
    sb.flag1 = read_32bit(0x14, streamFile); /* unknown, usually -1 but can be others (0/1/2/etc) */
    sb.flag2 = read_32bit(0x18, streamFile); /* unknown, usually -1 but can be others  */

    ok = config_sb_header_version(&sb, streamFile);
    if (!ok) {
        VGM_LOG("UBI SB: unknown SB version+platform\n");
        goto fail;
    }

    sb.section1_offset = 0x1c;
    sb.section2_offset = sb.section1_offset + sb.section1_entry_size * sb.section1_num;
    sb.extra_section_offset = sb.section2_offset + sb.section2_entry_size * sb.section2_num;
    sb.section3_offset = sb.extra_section_offset + sb.extra_size;
    sb.sounds_offset = sb.section3_offset + sb.section3_entry_size * sb.section3_num;
    sb.is_map = 0;

    /* use smaller I/O buffer for performance, as this read lots of small headers all over the place */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;

    /* main parse */
    if (!parse_sb_header(&sb, streamTest, target_stream))
        goto fail;

    close_streamfile(streamTest);
    return init_vgmstream_ubi_sb_main(&sb, streamFile);

fail:
    close_streamfile(streamTest);
    return NULL;
}

/* .SMx - essentially a set of SBx files, one per map, compiled into one file */
VGMSTREAM * init_vgmstream_ubi_sm(STREAMFILE *streamFile) {
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    //int16_t(*read_16bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = { 0 };
    size_t map_entry_size;
    int ok, i;
    int target_stream = streamFile->stream_index;

    /* check extension (number represents the platform, see later) */
    if (!check_extensions(streamFile, "sm0,sm1,sm2,sm3,sm4,sm5,sm6,sm7,lm0,lm1,lm2,lm3,lm4,lm5,lm6,lm7"))
        goto fail;

    if (target_stream == 0) target_stream = 1;

    /* sigh... PSP hijacks not one but *two* platform indexes */
    /* please add any PSP game versions under sb4 and sb5 sections so we can properly identify platform */
    sb.version = read_32bitLE(0x00, streamFile);

    if (check_extensions(streamFile, "sm0,lm0")) {
        sb.platform = UBI_PC;
    } else if (check_extensions(streamFile, "sm1,lm1")) {
        sb.platform = UBI_PS2;
    } else if (check_extensions(streamFile, "sm2,lm2")) {
        sb.platform = UBI_XBOX;
    } else if (check_extensions(streamFile, "sm3,lm3")) {
        sb.platform = UBI_GC;
    } else if (check_extensions(streamFile, "sm4,lm4")) {
        switch (sb.version) {
            case 0x0012000C:  /* Splinter Cell: Essentials (2006) */
                sb.platform = UBI_PSP;
                break;
            default:
                sb.platform = UBI_X360;
                break;
        }
    } else if (check_extensions(streamFile, "sm5,lm5")) {
        switch (sb.version) {
            case 0x00190001: /* TMNT (2007) */
            case 0x00190005: /* Surf's Up (2007) */
                sb.platform = UBI_PSP;
                break;
            default:
                sb.platform = UBI_3DS;
                break;
        }
    } else if (check_extensions(streamFile, "sm6,lm6")) {
        sb.platform = UBI_PS3;
    } else if (check_extensions(streamFile, "sm7,lm7")) {
        sb.platform = UBI_WII;
    } else {
        goto fail;
    }

    sb.big_endian = (sb.platform == UBI_GC ||
        sb.platform == UBI_PS3 ||
        sb.platform == UBI_X360 ||
        sb.platform == UBI_WII);
    if (sb.big_endian) {
        read_32bit = read_32bitBE;
        //read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        //read_16bit = read_16bitLE;
    }

    sb.is_map = 1;
    sb.version = read_32bit(0x00, streamFile);
    sb.map_header_offset = read_32bit(0x04, streamFile);
    sb.map_num = read_32bit(0x08, streamFile);

    ok = config_sb_header_version(&sb, streamFile);
    if (!ok || sb.map_version == 0) {
        VGM_LOG("UBI SB: unknown SM version+platform\n");
        goto fail;
    }

    /* use smaller I/O buffer for performance, as this read lots of small headers all over the place */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;

    map_entry_size = (sb.map_version < 2) ? 0x30 : 0x34;

    for (i = 0; i < sb.map_num; i++) {
        /* basic layout:
         * 0x00 - map type
         * 0x04 - zero
         * 0x08 - map section offset
         * 0x0c - map section size
         * 0x10 - map name (20 byte) */
        off_t offset = sb.map_header_offset + i * map_entry_size;
        sb.map_offset = read_32bit(offset + 0x08, streamFile);
        read_string(sb.map_name, sizeof(sb.map_name), offset + 0x10, streamFile);
        /* latest format has another unknown long here or maybe string buffer is 4 bytes bigger */

        /* parse map section header */
        sb.section1_offset = read_32bit(sb.map_offset + 0x04, streamFile) + sb.map_offset;
        sb.section1_num = read_32bit(sb.map_offset + 0x08, streamFile);
        sb.section2_offset = read_32bit(sb.map_offset + 0x0c, streamFile) + sb.map_offset;
        sb.section2_num = read_32bit(sb.map_offset + 0x10, streamFile);

        if (sb.map_version < 3) {
            sb.section3_offset = read_32bit(sb.map_offset + 0x14, streamFile) + sb.map_offset;
            sb.section3_num = read_32bit(sb.map_offset + 0x18, streamFile);
            sb.extra_section_offset = read_32bit(sb.map_offset + 0x1c, streamFile) + sb.map_offset;
            sb.extra_size = read_32bit(sb.map_offset + 0x20, streamFile);
        } else {
            /* latest map format has another section with sounds after section 2 */
            sb.section4_offset = read_32bit(sb.map_offset + 0x14, streamFile);
            sb.section4_num = read_32bit(sb.map_offset + 0x18, streamFile);
            sb.section3_offset = read_32bit(sb.map_offset + 0x1c, streamFile) + sb.map_offset;
            sb.section3_num = read_32bit(sb.map_offset + 0x20, streamFile);
            sb.extra_section_offset = read_32bit(sb.map_offset + 0x24, streamFile) + sb.map_offset;
            sb.extra_size = read_32bit(sb.map_offset + 0x28, streamFile);

            /* Let's just merge it with section 2 */
            sb.section2_num += sb.section4_num;

            /* for some reason, this is relative to section 4 here */
            sb.extra_section_offset += sb.section4_offset;
        }

        if (!parse_sb_header(&sb, streamTest, target_stream))
            goto fail;
    }

    if (sb.total_streams == 0) {
        VGM_LOG("UBI SB: no streams\n");
        goto fail;
    }

    if (target_stream < 0 || target_stream > sb.total_streams) {
        VGM_LOG("UBI SB: wrong target stream (target=%i, total=%i)\n", target_stream, sb.total_streams);
        goto fail;
    }

    close_streamfile(streamTest);
    return init_vgmstream_ubi_sb_main(&sb, streamFile);

fail:
    close_streamfile(streamTest);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_sb_main(ubi_sb_header *sb, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *streamData = NULL;
    off_t start_offset;
    int loop_flag = 0;

    /* open external stream if needed */
    if (sb->autodetect_external) { /* works most of the time but could give false positives */
        VGM_LOG("UBI SB: autodetecting external stream '%s'\n", sb->extra_name);

        streamData = open_streamfile_by_filename(streamFile,sb->extra_name);
        if (!streamData) {
            streamData = streamFile; /* assume internal */
            if (sb->stream_size > get_streamfile_size(streamData)) {
                VGM_LOG("UBI SB: expected external stream\n");
                goto fail;
            }
        } else {
            sb->is_external = 1;
        }
    }
    else if (sb->is_external) {
        streamData = open_streamfile_by_filename(streamFile,sb->extra_name);
        if (!streamData) {
            VGM_LOG("UBI SB: external stream '%s' not found\n", sb->extra_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }

    start_offset = sb->stream_offset;
    //;VGM_LOG("start offset=%lx, external=%i\n", start_offset, sb->is_external);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sb->channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sb->sample_rate;
    vgmstream->num_streams = sb->total_streams;
    vgmstream->stream_size = sb->stream_size;
    vgmstream->meta_type = meta_UBI_SB;


    switch(sb->codec) {
        case UBI_ADPCM:
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = ubi_ima_bytes_to_samples(sb->stream_size, sb->channels, streamData, start_offset);
            break;

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE even on Wii */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(sb->stream_size, sb->channels, 16);
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (sb->stream_type == 0x00) ? sb->stream_size / sb->channels : 0x10; /* TODO: needs testing */
            vgmstream->num_samples = ps_bytes_to_samples(sb->stream_size, sb->channels) ;
            break;

        case RAW_XBOX:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(sb->stream_size, sb->channels);
            break;

        case RAW_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb->stream_size / sb->channels;
            vgmstream->num_samples = dsp_bytes_to_samples(sb->stream_size, sb->channels);

            {
                off_t coefs_offset = sb->extra_offset;
                coefs_offset += 0x10; /* entry size is 0x40 (first/last 0x10 = unknown), per channel */

                dsp_read_coefs_be(vgmstream,streamFile,coefs_offset, 0x40);
            }
            break;

        case FMT_VAG:
            /* skip VAG header (some sb4 use VAG and others raw PSX) */
            if (read_32bitBE(start_offset, streamData) == 0x56414770) { /* "VAGp" */
                start_offset += 0x30;
                sb->stream_size  -= 0x30;
            }

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb->stream_size / sb->channels;
            vgmstream->num_samples = ps_bytes_to_samples(sb->stream_size, sb->channels);

            break;

#ifdef VGM_USE_FFMPEG
        case FMT_AT3: {
            ffmpeg_codec_data *ffmpeg_data;

            /* skip weird value (3, 4) in Brothers in Arms: D-Day (PSP) */
            if (read_32bitBE(start_offset+0x04,streamData) == 0x52494646) {
                start_offset += 0x04;
                sb->stream_size -= 0x04;
            }

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (sb->stream_samples == 0) /* sometimes not known */
                sb->stream_samples = ffmpeg_data->totalSamples;
            vgmstream->num_samples = sb->stream_samples;
            if (sb->stream_samples != ffmpeg_data->totalSamples) {
                VGM_LOG("UBI SB: header samples differ (%i vs %i)\n", sb->stream_samples, (size_t)ffmpeg_data->totalSamples);
                goto fail;
            }

            if (ffmpeg_data->skipSamples <= 0) /* in case FFmpeg didn't get them */
                ffmpeg_set_skip_samples(ffmpeg_data, riff_get_fact_skip_samples(streamData, start_offset));
            break;
        }

        case RAW_AT3: {
            ffmpeg_codec_data *ffmpeg_data;
            uint8_t buf[0x100];
            int32_t bytes, block_size, encoder_delay, joint_stereo;

            block_size = 0x98 * sb->channels;
            joint_stereo = 0;
            encoder_delay = 0x00; /* TODO: this is incorrect */

            bytes = ffmpeg_make_riff_atrac3(buf, 0x100, sb->stream_samples, sb->stream_size, sb->channels, sb->sample_rate, block_size, joint_stereo, encoder_delay);
            ffmpeg_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, sb->stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->num_samples = sb->stream_samples;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case FMT_XMA1: {
            ffmpeg_codec_data *ffmpeg_data;
            uint8_t buf[0x100];
            uint32_t sec1_num, sec2_num, sec3_num, num_frames, bits_per_frame;
            uint8_t flag;
            size_t bytes, chunk_size, header_size, data_size;
            off_t header_offset;

            chunk_size = 0x20;

            /* formatted XMA sounds have a strange custom header */
            header_offset = start_offset; /* XMA fmt chunk at the start */
            flag = read_8bit(header_offset + 0x20, streamData);
            sec2_num = read_32bitBE(header_offset + 0x24, streamData); /* number of XMA frames */
            sec1_num = read_32bitBE(header_offset + 0x28, streamData);
            sec3_num = read_32bitBE(header_offset + 0x2c, streamData);
            num_frames = sec2_num;
            bits_per_frame = 4;

            if (flag == 0x04) {
                sec1_num--;
                sec2_num += 4;
            } else if (flag == 0x02) {
                bits_per_frame = 2;
            }

            header_size = 0x30;
            header_size += sec1_num * 0x04;
            header_size += align_size_to_block(sec2_num * bits_per_frame, 32) / 8; /* bitstream with 4 or 2 bits for each frame */
            header_size += sec3_num * 0x08;
            start_offset += header_size;
            data_size = num_frames * 0x800;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, data_size, streamData, 1);

            ffmpeg_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, data_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = sb->stream_samples;
            vgmstream->stream_size = data_size;
            break;
        }

        case RAW_XMA1: {
            ffmpeg_codec_data *ffmpeg_data;
            uint8_t buf[0x100];
            size_t bytes, chunk_size;
            off_t header_offset;
            
            VGM_ASSERT(sb->is_external, "Ubi SB: Raw XMA used for external sound\n");

            /* get XMA header from extra section */
            chunk_size = 0x20;
            header_offset = sb->xma_header_offset;
            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, sb->stream_size, streamFile, 1);

            ffmpeg_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, sb->stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = sb->stream_samples;
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = sb->stream_samples; /* ffmpeg_data->totalSamples */
            VGM_ASSERT(sb->stream_samples != ffmpeg_data->totalSamples, "UBI SB: header samples differ\n");
            break;
        }

#endif
        default:
            VGM_LOG("UBI SB: unknown codec\n");
            goto fail;
    }

    strcpy(vgmstream->stream_name, sb->stream_name);

    /* open the file for reading (can be an external stream, different from the current .sb0) */
    if ( !vgmstream_open_stream(vgmstream, streamData, start_offset) )
        goto fail;

    if (sb->is_external && streamData) close_streamfile(streamData);
    return vgmstream;

fail:
    if (sb->is_external && streamData) close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;

}


/* debug stuff, for now */
static void parse_descriptor_subtype(ubi_sb_header * sb, uint32_t descriptor_subtype, off_t offset) {

    /* type may be flags? */
    /* all types may contain memory garbage, making it harder to identify */

    switch(descriptor_subtype) {
        /* standard audio */
        case 0x01: sb->subtypes[0x01]++; break;

        /* audio/garbage? [Splinter Cell: Chaos Theory] */
        /* audio/garbage? [Surf's Up-map] */
        /* audio/garbage? [TMNT-map] */
        /* config? [Brothers in Arms: D-Day] */
        /* config? [Surf's Up-bank] */
        case 0x02: sb->subtypes[0x02]++; break;

        /* audio/garbage? [Splinter Cell: Chaos Theory] */
        /* audio/garbage? [TMNT-map] */
        /* config? [Prince of Persia: The Two Thrones] */
        case 0x03: sb->subtypes[0x03]++; break;

        /* audio/garbage? [Splinter Cell: Chaos Theory] */
        /* audio/garbage? [TMNT-map] */
        /* config? [Rainbow Six 3] */
        /* config? [Prince of Persia: Warrior Within] */
        /* config? [Prince of Persia: The Two Thrones] */
        /* config? [TMNT-bank] */
        /* config? [Surf's Up-map] */
        /* audio/garbage? [Myst IV demo] */
        /* config? [Brothers in Arms: D-Day] */
        /* config? [Surf's Up-bank] */
        case 0x04: sb->subtypes[0x04]++; break;

        /* audio/garbage? [Splinter Cell: Chaos Theory] */
        /* audio/garbage? [TMNT-map] */
        /* config? [Prince of Persia: Warrior Within] */
        /* config? [Prince of Persia: The Two Thrones] */
        /* config? [Prince of Persia: Revelations] */
        /* config? [TMNT-bank] */
        case 0x05: sb->subtypes[0x05]++; break;

        /* layer? [Surf's Up-map] */
        /* layer [TMNT-bank] */
        /* layer [TMNT-map] */
        case 0x06: sb->subtypes[0x06]++; break;

        /* audio/garbage? [Splinter Cell: Chaos Theory] */
        /* config? [Brothers in Arms: D-Day] */
        /* config? [Surf's Up-bank] */
        case 0x07: sb->subtypes[0x07]++; break;

        /* audio/garbage? [Splinter Cell: Chaos Theory] */
        /* audio/garbage? [TMNT-map] */
        /* config? [Prince of Persia: Warrior Within] */
        /* config? [Prince of Persia: The Two Thrones] */
        /* config? [TMNT-bank] */
        /* config? [Surf's Up-map] */
        /* config? [Brothers in Arms: D-Day] */
        /* config? [Surf's Up-bank] */
        case 0x08: sb->subtypes[0x08]++; break;

        /* related to voices?  [Prince of Persia: Sands of Time] */
        /* config? [Rainbow Six 3] */
        /* config? [Myst IV demo] */
        case 0x0a: sb->subtypes[0x0a]++; break;

        /* config? [Prince of Persia: Sands of Time] */
        /* config? [Rainbow Six 3] */
        case 0x0c: sb->subtypes[0x0c]++; break;

        /* layer [Prince of Persia: Sands of Time] */
        /* layer [Rainbow Six 3] */
        case 0x0d: sb->subtypes[0x0d]++; break;

        /* config? [Rainbow Six 3] */
        /* config? [Myst IV demo] */
        case 0x0f: sb->subtypes[0x0f]++; break;

        default:
            VGM_LOG("UBI SB: unknown subtype %x at %x size %x\n", descriptor_subtype, (uint32_t)offset, sb->section2_entry_size);
            break; //goto fail;
    }

    //;VGM_ASSERT(descriptor_subtype != 0x01, "UBI SB: subtype %x at %x size %x\n", descriptor_subtype, (uint32_t)offset, sb->section2_entry_size);


    /* 0x06 layer [TMNT-bank] */
    /* - subtype header:
     * 0x1c: external flag?
     * 0x20: layers/channels?
     * 0x24: layers/channels?
     * 0x28: config? same for all
     * 0x2c: stream size
     * 0x30: stream offset
     * 0x38: always 0x037c
     * (todo sample rate?)
     *
     * - layer header at stream_offset:
     * 0x00: version? (0x08000B00)
     * 0x04: 0x0e?
     * 0x08: layers/channels?
     * 0x0c: blocks count
     * 0x10: block header size
     * 0x14: size of header sizes + codec headers
     * 0x18: block size
     * - per layer:
     * 0x00: layer header size
     * - per layer
     * 0x00~0x20: standard Ubi IMA header (version 0x05)
     *
     * - blocked data:
     * 0x00: always 0x03
     * 0x04: block size
     * - per layer:
     * 0x00: layer data size (varies between blocks, and one layer may have more than other)
     */

    /* 0x06 layer [TMNT-map] */
    /* - subtype header:
     * 0x20: layers/channels?
     * 0x24: layers/channels * 2?
     * 0x28: config? same for all
     * 0x2c: stream size
     * 0x30: stream offset
     * 0x38: 0x01D0/0118/etc
     * 0x40: external flag?
     * 0x48: codec?
     * 0x54: codec?
     * (todo sample rate?)
     *
     * - layer header at stream_offset:
     * - blocked data:
     * same as TMNT-bank, but codec header size is 0
     */

    //todo Surf's Up-map

    /* 0x0d layer [Prince of Persia: Sands of Time] */
    /* 0x0d layer [Rainbow Six 3] */ //todo also check layer header
    /* - subtype header (bizarrely but thankfully doesn't change between platforms):
     * 0x1c: sample rate * layers
     * 0x20: layers/channels?
     * 0x2c: external flag?
     * 0x30: external name
     * 0x58: stream offset
     * 0x5c: original rate * layers?
     * 0x60: stream size (not including padding)
     * 0x64: number of samples
     *
     * - layer header at stream_offset:
     * 0x00: version? (0x04000000)
     * 0x04: layers
     * 0x08: stream size (not including padding)
     * 0x0c: blocks count
     * 0x10: block header size
     * 0x14: block size
     * 0x18: ?
     * 0x1c: size of next section?
     * - per layer:
     * 0x00: layer header size
     * codec header per layer
     * 0x00~0x20: standard Ubi IMA header (version 0x05)
     *
     * - blocked data:
     * 0x00: block number (from 0x01 to block_count)
     * 0x04: current offset (within stream_offset)
     * 0x08: always 0x03
     * - per layer:
     * 0x00: layer data size (varies between blocks, and one layer may have more than other)
     */

}

static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile, int target_stream) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int i, j, k, current_type = -1, current_id = -1, bank_streams = 0, prev_streams;

    if (sb->big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    prev_streams = sb->total_streams;

    /* find target stream info in section2 */
    for (i = 0; i < sb->section2_num; i++) {
        off_t offset = sb->section2_offset + sb->section2_entry_size*i;
        uint32_t descriptor_subtype;

        descriptor_subtype = read_32bit(offset + 0x04, streamFile);
        parse_descriptor_subtype(sb, descriptor_subtype, offset);

        /* ignore non-audio entries */
        if (descriptor_subtype != 0x01)
            continue;
        //;VGM_LOG("UBI SB: type at %lx\n", offset);

        /* weird case when there is no internal substream ID and just seem to rotate every time type changes, joy */
        if (sb->has_rotating_ids) { /* assumes certain configs can't happen in this case */
            int current_is_external = 0;
            int type = read_32bit(offset + sb->stream_type_offset, streamFile);

            if (sb->external_flag_offset) {
                current_is_external = read_32bit(offset + sb->external_flag_offset, streamFile);
            } else if (sb->has_extra_name_flag && read_32bit(offset + sb->extra_name_offset, streamFile) != 0xFFFFFFFF) {
                current_is_external = 1; /* -1 in extra_name means internal */
            }

            if (!current_is_external) {
                if (sb->is_map) {
                    if (current_type == -1)
                        current_type = type;
                    if (current_id == -1) /* they seem to always start with 0 in maps */
                        current_id = 0x00; 

                    if (!current_is_external) {
                        if (current_type != type) {
                            current_type = type;
                            current_id++; /* rotate */
                            /* we'll warp it around later when parsing section 3 */
                        }
                    }
                } else {
                    if (current_type == -1)
                        current_type = type;
                    if (current_id == -1) /* use first ID in section3 */
                        current_id = read_32bit(sb->section3_offset + 0x00, streamFile);

                    if (!current_is_external) {
                        if (current_type != type) {
                            current_type = type;
                            current_id++; /* rotate */
                            if (current_id >= sb->section3_num)
                                current_id = 0; /* reset */
                        }
                    }
                }
            }
        }

        /* update streams (total_stream also doubles as current) */
        sb->total_streams++;
        bank_streams++;
        if (sb->total_streams != target_stream)
            continue;
        //;VGM_LOG("target at offset=%lx (size=%x)\n", offset, sb->section2_entry_size);

        /* parse audio entry based on config */
        sb->header_offset  = offset;
        sb->header_idx     = i;

        sb->header_id      = read_32bit(offset + 0x00, streamFile); /* 16b+16b group+sound id */
        sb->header_type    = read_32bit(offset + 0x04, streamFile);
        sb->stream_size    = read_32bit(offset + sb->stream_size_offset, streamFile);
        sb->extra_offset   = read_32bit(offset + sb->extra_pointer_offset, streamFile) + sb->extra_section_offset; /* within the extra section */
        sb->stream_offset  = read_32bit(offset + sb->stream_pointer_offset, streamFile); /* within the data section */
        sb->channels       = (sb->has_short_channels) ?
                   (uint16_t)read_16bit(offset + sb->channels_offset, streamFile) :
                   (uint32_t)read_32bit(offset + sb->channels_offset, streamFile);
        sb->sample_rate    = read_32bit(offset + sb->sample_rate_offset, streamFile);
        sb->stream_type    = read_32bit(offset + sb->stream_type_offset, streamFile);

        /* Some games may store number of samples at different locations */
        if (sb->samples_flag_offset &&
            read_32bit(offset + sb->samples_flag_offset, streamFile) != 0) {
            sb->stream_samples = read_32bit(offset + sb->num_samples_offset2, streamFile);
        } else if (sb->num_samples_offset) {
            sb->stream_samples = read_32bit(offset + sb->num_samples_offset, streamFile);
        }

        if (sb-> has_rotating_ids) {
            sb->stream_id  = current_id;
        } else if (sb->stream_id_offset) {
            sb->stream_id  = read_32bit(offset + sb->stream_id_offset, streamFile);
        }

        /* external stream name can be found in the header (first versions) or the extra table (later versions) */
        if (sb->stream_name_offset) {
            read_string(sb->extra_name, sb->stream_name_size, offset + sb->stream_name_offset, streamFile);
        } else {
            sb->stream_name_offset = read_32bit(offset + sb->extra_name_offset, streamFile);
            read_string(sb->extra_name, sb->stream_name_size, sb->extra_section_offset + sb->stream_name_offset, streamFile);
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

            if (sb->extra_name[0] == '\0')
                sb->autodetect_external = 0; /* no name */
            if (sb->extra_size > 0 && sb->stream_name_offset > sb->extra_size)
                sb->autodetect_external = 0; /* name outside extra table == is internal */
        }

        /* build a full name for stream */
        if (sb->is_map) {
            if (sb->extra_name[0]) {
                snprintf(sb->stream_name, sizeof(sb->stream_name), "%s/%d/%s", sb->map_name, bank_streams, sb->extra_name);
            } else {
                snprintf(sb->stream_name, sizeof(sb->stream_name), "%s/%d", sb->map_name, bank_streams);
            }
        } else {
            strncpy(sb->stream_name, sb->extra_name, sizeof(sb->stream_name));
        }
    }

#if 0
    {
        int i;
        VGM_LOG("UBI subtypes: ");
        for (i = 0; i < 16; i++) {
            VGM_ASSERT(sb->subtypes[i], "%02x=%i ",i,sb->subtypes[i]);
        }
        VGM_LOG("\n");
    }
#endif

    if (sb->is_map) {
        if (bank_streams == 0 || target_stream <= prev_streams || target_stream > sb->total_streams)
            return 1; /* Target stream is not in this map */
    } else {
        if (sb->total_streams == 0) {
            VGM_LOG("UBI SB: no streams\n");
            goto fail;
        }

        if (target_stream < 0 || target_stream > sb->total_streams) {
            VGM_LOG("UBI SB: wrong target stream (target=%i, total=%i)\n", target_stream, sb->total_streams);
            goto fail;
        }
    }

    if (!(sb->stream_id_offset || sb->has_rotating_ids || sb->is_map) && sb->section3_num > 1) {
        VGM_LOG("UBI SB: unexpected number of internal stream groups %i\n", sb->section3_num);
        goto fail;
    }


    /* happens in some versions */
    if (sb->stream_type > 0xFF) {
        VGM_LOG("UBI SB: garbage in stream_type\n");
        sb->stream_type = 0;
    }

    /* guess codec */
    switch (sb->stream_type) {
        case 0x00: /* platform default (rarely external) */
            switch (sb->platform) {
                case UBI_PC:
                    sb->codec = RAW_PCM;
                    break;

                case UBI_PS2:
                    sb->codec = RAW_PSX;
                    break;

                case UBI_PSP:
                    if (check_extensions(streamFile, "sb4,sm4")) {
                        sb->codec = FMT_VAG;
                    } else {
                        sb->codec = RAW_PSX;
                    }
                    break;

                case UBI_XBOX:
                    sb->codec = RAW_XBOX;
                    break;

                case UBI_GC:
                case UBI_WII:
                    sb->codec = RAW_DSP;
                    break;

                case UBI_X360:
                    sb->codec = RAW_XMA1;
                    break;
#if 0
                case UBI_PS3:
                    /* Need to confirm */
                    sb->codec = RAW_AT3;
                    break;
#endif
                default:
                    VGM_LOG("UBI SB: unknown internal format\n");
                    goto fail;

            }
            break;

        case 0x01: /* PCM (Wii, rarely used) */
            sb->codec = RAW_PCM;
            break;

        case 0x02: /* PS ADPCM (PS3) */
            sb->codec = RAW_PSX;
            break;

        case 0x03: /* Ubi ADPCM (main external stream codec, has subtypes) */
            sb->codec = UBI_ADPCM;
            break;

        case 0x04: /* Ogg (later PC games) */
            sb->codec = FMT_OGG;
            break;

        case 0x05: /* AT3 (PSP, PS3) or XMA1 (X360) */
            switch (sb->platform) {
                case UBI_X360:
                    sb->codec = FMT_XMA1;
                    break;
                case UBI_PS3:
                case UBI_PSP:
                    sb->codec = FMT_AT3;
                    break;
                default:
                    VGM_LOG("UBI SB: unknown codec for stream_type %x\n", sb->stream_type);
                    goto fail;
            }
            break;

        case 0x06: /* PS ADPCM (later PS3 games?) */
            sb->codec = RAW_PSX;
            break;

        case 0x07:
            sb->codec = RAW_AT3;
            break;

        case 0x08:
            sb->codec = FMT_AT3;
            break;

        default:
            VGM_LOG("UBI SB: unknown stream_type %x\n", sb->stream_type);
            goto fail;
    }

    if (sb->codec == RAW_XMA1) {
        /* this field is only seen in X360 games, points at XMA1 header in extra section */
        sb->xma_header_offset = read_32bit(sb->header_offset + sb->xma_pointer_offset, streamFile) + sb->extra_section_offset;
    }

    /* uncommon but possible */
    //VGM_ASSERT(sb->is_external && sb->section3_num != 0, "UBI SS: mixed external and internal streams\n");

    /* seems that can be safely ignored */
    //VGM_ASSERT(sb->is_external && sb->stream_id_offset && sb->stream_id > 0, "UBI SB: unexpected external stream with stream id\n");

    /* section 3: internal stream info */
    if (!sb->is_external) {
        if (sb->is_map) {
            /* maps store internal sounds offsets in a separate table, find the matching entry */
            for (i = 0; i < sb->section3_num; i++) {
                off_t offset = sb->section3_offset + 0x14 * i;
                off_t table_offset = read_32bit(offset + 0x04, streamFile) + sb->section3_offset;
                uint32_t table_num = read_32bit(offset + 0x08, streamFile);
                off_t table2_offset = read_32bit(offset + 0x0c, streamFile) + sb->section3_offset;
                uint32_t table2_num = read_32bit(offset + 0x10, streamFile);

                for (j = 0; j < table_num; j++) {
                    int idx = read_32bit(table_offset + 0x08 * j + 0x00, streamFile) & 0x0000FFFF;

                    if (idx == sb->header_idx) {
                        if (!(sb->stream_id_offset || sb->has_rotating_ids) && table2_num > 1) {
                            VGM_LOG("UBI SB: unexpected number of internal stream groups %i\n", table2_num);
                            goto fail;
                        }

                        if (sb->has_rotating_ids) {
                            sb->stream_id %= table2_num;
                        }

                        sb->stream_offset = read_32bit(table_offset + 0x08 * j + 0x04, streamFile);
                        for (k = 0; k < table2_num; k++) {
                            /* entry layout:
                             * 0x00 - group ID
                             * 0x04 - size with padding included
                             * 0x08 - size without padding
                             * 0x0c - absolute offset */
                            uint32_t id = read_32bit(table2_offset + 0x10 * k + 0x00, streamFile);
                            if (id == sb->stream_id) {
                                sb->stream_offset += read_32bit(table2_offset + 0x10 * k + 0x0c, streamFile);
                                break;
                            }
                        }
                        break;
                    }
                }

                if (sb->stream_offset)
                    break;
            }
        } else {
            sb->stream_offset += sb->sounds_offset;

            if ((sb->stream_id_offset || sb->has_rotating_ids) && sb->section3_num > 1) {
                /* internal sounds are split into groups based on their type with their offsets being relative to group start
                 * this table contains sizes of each group, adjust offset based on group ID of our sound */
                for (i = 0; i < sb->section3_num; i++) {
                    off_t offset = sb->section3_offset + sb->section3_entry_size * i;

                    /* table has unordered ids+size, so if our id doesn't match current data offset must be beyond */
                    if (read_32bit(offset + 0x00, streamFile) == sb->stream_id)
                        break;
                    sb->stream_offset += read_32bit(offset + 0x04, streamFile);
                }
            }
        }
    }

    return 1;
fail:
    return 0;
}


static int config_sb_header_version(ubi_sb_header * sb, STREAMFILE *streamFile) {
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

    /* this is same in all games since ~2003 */
    sb->stream_size_offset        = 0x08;
    sb->extra_pointer_offset      = 0x0c;
    sb->stream_pointer_offset     = 0x10;

#if 0
    /* Donald Duck: Goin' Quackers (2002)(GC)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_GC) {
        /* Stream types:
         * 0x01: Nintendo DSP ADPCM
         * 0x08: unsupported codec, looks like standard IMA with custom 0x29 bytes header
         */
        sb->section1_entry_size = 0x40;
        sb->section2_entry_size = 0x6c;

        sb->map_version = 1;

        sb->stream_size_offset    = 0x0c;
        sb->extra_pointer_offset  = 0x10;
        sb->stream_pointer_offset = 0x14;

        sb->external_flag_offset = 0x30;
        sb->stream_id_offset     = 0x34;
        sb->num_samples_offset   = 0x48;
        sb->sample_rate_offset   = 0x50;
        sb->channels_offset      = 0x56;
        sb->stream_type_offset   = 0x58;
        sb->stream_name_offset   = 0x5c;

        sb->has_short_channels = 1;
        return 1;
    }

    /* Splinter Cell (2002)(PC)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(PC)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_PC) {
        /* Stream types:
         * 0x01: PCM
         * 0x02: unsupported codec, appears to be another version of Ubi IMA
         * 0x04: Ubi IMA v3 (not Vorbis)
         */
        sb->section1_entry_size = 0x58;
        sb->section2_entry_size = 0x80;

        sb->map_version = 1;

        sb->stream_size_offset    = 0x0c;
        sb->extra_pointer_offset  = 0x10;
        sb->stream_pointer_offset = 0x14;

        sb->external_flag_offset = 0x28;
        sb->stream_id_offset     = 0x2c;
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4a;
        sb->stream_type_offset   = 0x4c;
        sb->stream_name_offset   = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        return 1;
    }
#endif

    /* Prince of Persia: Sands of Time (2003)(PC)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PC) || /* (not sure if exists, just in case) */
        (sb->version == 0x000A0004 && sb->platform == UBI_PC)) { /* main game */
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

    /* Prince of Persia: Sands of Time (2003)(PS2)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PS2) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_PS2)) { /* main game */
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

    /* Prince of Persia: Sands of Time (2003)(Xbox)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_XBOX) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_XBOX)) { /* main game */
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

    /* Tom Clancy's Rainbow Six 3 (2003)(PS2)-bank */
    if (sb->version == 0x000A0007 && sb->platform == UBI_PS2) {
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

    /* Prince of Persia: Sands of Time (2003)(GC)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_GC) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_GC)) { /* main game */
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

    /* Myst IV Demo (2004)(PC) (final game is different)-bank */
    if (sb->version == 0x00100000 && sb->platform == UBI_PC) {
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

    /* Prince of Persia: Warrior Within (2004)(PC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_PC) {
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

    /* Prince of Persia: Warrior Within (2004)(PS2)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_PS2) {
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

    /* Prince of Persia: Warrior Within (2004)(Xbox)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_XBOX) {
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

    /* Prince of Persia: Warrior Within (2004)(GC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_GC) {
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
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "BIAAUDIO.SP4");
        if (streamTest) {
            is_biadd_psp = 1;
            close_streamfile(streamTest);
        }
    }

    /* Prince of Persia: Revelations (2005)(PSP)-bank */
    /* Splinter Cell: Essentials (2006)(PSP)-map */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && !is_biadd_psp) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x84;

        sb->map_version = 2;

        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->stream_name_offset   = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Brothers in Arms - D-Day (2006)(PSP)-bank */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && is_biadd_psp) {
        sb->section1_entry_size = 0x80;
        sb->section2_entry_size = 0x94;

        sb->stream_id_offset     = 0x2c;
        sb->external_flag_offset = 0x24;
        sb->samples_flag_offset  = 0x28;
        sb->num_samples_offset   = 0x30;
        sb->num_samples_offset2  = 0x38;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->stream_name_offset   = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PC) {
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

    /* Prince of Persia: The Two Thrones (2005)(PS2)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PS2) {
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

    /* Prince of Persia: The Two Thrones (2005)(Xbox)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_XBOX) {
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

    /* Prince of Persia: The Two Thrones (2005)(GC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_GC) {
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

    /* Splinter Cell: Chaos Theory (2005)(PC)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x60;

        sb->map_version = 2;

        sb->external_flag_offset = 0x24;
        sb->num_samples_offset   = 0x30;
        sb->sample_rate_offset   = 0x44;
        sb->channels_offset      = 0x4c;
        sb->stream_type_offset   = 0x50;
        sb->extra_name_offset    = 0x54;

        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(Xbox)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x4c;

        sb->map_version = 2;

        sb->external_flag_offset = 0;
        sb->num_samples_offset   = 0x18;
        sb->stream_id_offset     = 0;
        sb->sample_rate_offset   = 0x30;
        sb->channels_offset      = 0x38;
        sb->stream_type_offset   = 0x3c;
        sb->extra_name_offset    = 0x40;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1;
        return 1;
    }

#if 0
    /* Far cry: Instincts - Evolution (2006)(Xbox)-bank */
    if (sb->version == 0x00170000 && sb->platform == UBI_XBOX) {
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

    /* Red Steel (2006)(Wii)-bank */
    if (sb->version == 0x00180006 && sb->platform == UBI_WII) { /* same as 0x00150000 */
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

    /* Splinter Cell: Double Agent (2006)(PC)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x7c;

        sb->map_version = 3;

        sb->external_flag_offset = 0x2c;
        sb->stream_id_offset     = 0x34;
        sb->channels_offset      = 0x5c;
        sb->sample_rate_offset   = 0x54;
        sb->num_samples_offset   = 0x40;
        sb->num_samples_offset2  = 0x48;
        sb->stream_type_offset   = 0x60;
        sb->extra_name_offset    = 0x64;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(X360)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_X360) {
        sb->section1_entry_size  = 0x68;
        sb->section2_entry_size  = 0x78;

        sb->map_version = 3;

        sb->external_flag_offset = 0x2c;
        sb->stream_id_offset     = 0x30;
        sb->samples_flag_offset  = 0x34;
        sb->channels_offset      = 0x5c;
        sb->sample_rate_offset   = 0x54;
        sb->num_samples_offset   = 0x40;
        sb->num_samples_offset2  = 0x48;
        sb->stream_type_offset   = 0x60;
        sb->extra_name_offset    = 0x64;
        sb->xma_pointer_offset   = 0x70;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(Xbox)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

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

    /* Splinter Cell: Double Agent (2006)(GC)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->map_version = 3;

        sb->external_flag_offset = 0x28;
        sb->stream_id_offset     = 0x2c;
        sb->num_samples_offset   = 0x3c;
        sb->sample_rate_offset   = 0x50;
        sb->channels_offset      = 0x58;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x60;

        return 1;
    }

    /* Prince of Persia: Rival Swords (2007)(PSP)-bank */
    if (sb->version == 0x00180005 && sb->platform == UBI_PSP) {
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
    /* Rainbow Six Vegas (2007)(PSP)-bank */
    if (sb->version == 0x00180006 && sb->platform == UBI_PSP) {
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

    /* Prince of Persia: Rival Swords (2007)(Wii)-bank */
    if (sb->version == 0x00190003 && sb->platform == UBI_WII) {
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
    
    /* TMNT (2007)(PC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->external_flag_offset = 0x28;
        sb->stream_id_offset     = 0x2c;
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;

        return 1;
    }

    /* TMNT (2007)(PS2)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PS2) {
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

    /* TMNT (2007)(GC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_GC) { /* same as 0x00190003 */
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

     /* TMNT (2007)(X360)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->external_flag_offset = 0x28;
        sb->samples_flag_offset  = 0x30;
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->num_samples_offset2  = 0x50;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;
        sb->xma_pointer_offset   = 0x6c;

        return 1;
    }

    /* TMNT (2007)(PSP)-map */
    if (sb->version == 0x00190001 && sb->platform == UBI_PSP) {
        sb->section1_entry_size  = 0x48;
        sb->section2_entry_size  = 0x58;

        sb->map_version = 3;

        sb->external_flag_offset = 0;
        sb->channels_offset      = 0x28;
        sb->sample_rate_offset   = 0x2c;
        sb->num_samples_offset   = 0x34;
        sb->stream_type_offset   = 0x48;
        sb->extra_name_offset    = 0x44;

        sb->has_extra_name_flag = 1;
        return 1;
    }

    /* Surf's Up (2007)(PC)-bank */
    if (sb->version == 0x00190005 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->external_flag_offset = 0x28; /* maybe 0x2c */
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;

        return 1;
    }

    /* Surf's Up (2007)(PS3)-bank */
    /* Splinter Cell: Double Agent (2007)(PS3)-map */
    if (sb->version == 0x00190005 && sb->platform == UBI_PS3) {
        sb->section1_entry_size  = 0x68;
        sb->section2_entry_size  = 0x70;

        sb->map_version = 3;

        sb->external_flag_offset = 0x28;
        sb->stream_id_offset     = 0x2c;
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;

        return 1;
    }

    /* Surf's Up (2007)(X360)-bank */
    if (sb->version == 0x00190005 && sb->platform == UBI_X360) {
        sb->section1_entry_size  = 0x68;
        sb->section2_entry_size  = 0x70;

        sb->external_flag_offset = 0x28;
        sb->samples_flag_offset  = 0x30;
        sb->channels_offset      = 0x3c;
        sb->sample_rate_offset   = 0x40;
        sb->num_samples_offset   = 0x48;
        sb->num_samples_offset2  = 0x50;
        sb->stream_type_offset   = 0x5c;
        sb->extra_name_offset    = 0x58;
        sb->xma_pointer_offset   = 0x6c;

        return 1;
    }

    /* Surf's Up (2007)(PSP)-map */
    if (sb->version == 0x00190005 && sb->platform == UBI_PSP) {
        sb->section1_entry_size  = 0x48;
        sb->section2_entry_size  = 0x58;

        sb->map_version = 3;

        sb->external_flag_offset = 0;
        sb->channels_offset      = 0x28;
        sb->sample_rate_offset   = 0x2c;
        sb->num_samples_offset   = 0x34;
        sb->stream_type_offset   = 0x48;
        sb->extra_name_offset    = 0x44;

        sb->has_extra_name_flag = 1;
        return 1;
    }

    /* Splinter Cell Classic Trilogy HD (2011)(PS3)-map */
    if (sb->version == 0x001d0000 && sb->platform == UBI_PS3) {
        sb->section1_entry_size  = 0x5c;
        sb->section2_entry_size  = 0x80;

        sb->map_version = 3;

        sb->external_flag_offset = 0x28;
        sb->stream_id_offset     = 0x30;
        sb->samples_flag_offset  = 0x34;
        sb->channels_offset      = 0x44;
        sb->sample_rate_offset   = 0x4c;
        sb->num_samples_offset   = 0x54;
        sb->num_samples_offset2  = 0x5c;
        sb->stream_type_offset   = 0x68;
        sb->extra_name_offset    = 0x64;

        return 1;
    }

    return 0;
}
