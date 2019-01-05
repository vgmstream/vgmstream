#include "meta.h"
#include "../coding/coding.h"


typedef enum { UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_DSP, RAW_XBOX, FMT_VAG, FMT_AT3, RAW_AT3, FMT_XMA1, RAW_XMA1, FMT_OGG } ubi_sb_codec;
typedef enum { UBI_PC, UBI_PS2, UBI_XBOX, UBI_GC, UBI_X360, UBI_PSP, UBI_PS3, UBI_WII, UBI_3DS } ubi_sb_platform;
typedef struct {
    ubi_sb_platform platform;
    int big_endian;
    int total_streams;
    int is_external;
    int autodetect_external;
    ubi_sb_codec codec;

    /* map base header info */
    off_t map_start;
    off_t map_num;
    size_t map_entry_size;

    uint32_t map_type;
    uint32_t map_zero;
    off_t map_offset;
    off_t map_size;
    char map_name[255];
    uint32_t map_unknown;

    /* SB main/fixed info */
    uint32_t version;
    uint32_t version_empty;
    size_t section1_num;
    size_t section2_num;
    size_t section3_num;
    size_t section4_num;
    size_t sectionX_size;
    int flag1;
    int flag2;

    /* maps data config */
    int is_map;
    int map_version;

    /* header/stream info config */
    /* audio header varies slightly per game but not enough parse case by case,
     * instead we configure sizes and offsets to where each variable is */
    size_t section1_entry_size;
    size_t section2_entry_size;
    size_t section3_entry_size;
    size_t resource_name_size;
    off_t  cfg_stream_size;
    off_t  cfg_stream_offset;
    off_t  cfg_extra_offset;
    off_t  cfg_stream_id;
    off_t  cfg_stream_type;

    off_t  cfg_external_flag;   /* if the song is internal or external */
    off_t  cfg_samples_flag;    /* some headers have two possible locations for num_samples */
    off_t  cfg_samples_bitflag;
    off_t  cfg_num_samples;
    off_t  cfg_num_samples2;
    off_t  cfg_sample_rate;
    off_t  cfg_channels;
    off_t  cfg_stream_name;     /* where the resource name is within the header */
    off_t  cfg_extra_name;      /* where the resource name is within sectionX */
    off_t  cfg_xma_offset;
    int has_short_channels;     /* channels value can be 16b or 32b */
    int has_internal_names;     /* resource name doubles as internal name in some cases */
    int has_extra_name_flag;    /* if cfg_extra_name is set (since often extra_name = -1 is 'not set' and >= 0 is offset) */
    int has_rotating_ids;       /* stream id isn't set but is assigned using sequential rotation of sorts */
    //todo see if has_extra_name_flag can be removed

    /* derived */
    size_t section1_offset;
    size_t section2_offset;
    size_t section3_offset;
    size_t section4_offset;
    size_t sectionX_offset;
    size_t sounds_offset;

    /* header/stream info */
    uint32_t header_id;         /* 16b+16b group+sound id identifier (should be unique within sbX, but not smX) */
    uint32_t header_type;       /* audio type (we only need 'standard audio' or 'layered audio') */
    size_t stream_size;         /* size of the audio data */
    off_t stream_offset;        /* offset within the data section (internal) or absolute (external) to the audio */
    off_t extra_offset;         /* offset within sectionX to extra data */

    uint32_t stream_id;         /* internal resource id needed to locate info */
    uint32_t stream_type;       /* rough codec value */
    int num_samples;            /* usually only for external resources */
    int sample_rate;
    int channels;
    char resource_name[255];    /* filename to the external stream, or internal stream info for some games */
    char readable_name[255];    /* constructed name to show externally */

    int header_index;           /* position within section2 (considering all possible header types) */
    off_t header_offset;        /* offset of parsed audio header */
    off_t xma_header_offset;    /* XMA has some extra header stuff*/

    int types[16];              /* counts for each possible header types, for debugging */
} ubi_sb_header;

static VGMSTREAM * init_vgmstream_ubi_sb_main(ubi_sb_header *sb, STREAMFILE *streamFile);
static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile, int target_stream);
static int config_sb_platform(ubi_sb_header * sb, STREAMFILE *streamFile);
static int config_sb_version(ubi_sb_header * sb, STREAMFILE *streamFile);

/* .SBx - banks from Ubisoft's sound engine ("DARE" / "UbiSound Driver") games in ~2000-2008+ */
VGMSTREAM * init_vgmstream_ubi_sb(STREAMFILE *streamFile) {
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = { 0 };
    int ok;
    int target_stream = streamFile->stream_index;

    /* check extension (number represents the platform, see later) */
    if (!check_extensions(streamFile, "sb0,sb1,sb2,sb3,sb4,sb5,sb6,sb7"))
        goto fail;

    /* .sbX (sound bank) is a small multisong format (loaded in memory?) that contains SFX data
     * but can also reference .ss0/ls0 (sound stream) external files for longer streams.
     * A companion .sp0 (sound project) describes files and if it uses BANKs (.sb0) or MAPs (.sm0). */


    /* PLATFORM DETECTION */
    if (!config_sb_platform(&sb, streamFile))
        goto fail;
    if (sb.big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    if (target_stream == 0) target_stream = 1;

    /* use smaller I/O buffer for performance, as this read lots of small headers all over the place */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;


    /* SB HEADER */
    /* SBx layout: base header, section1, section2, extra section, section3, data (all except base header can be null) */
    sb.is_map = 0;
    sb.version       = read_32bit(0x00, streamFile); /* 16b+16b major/minor version */
    sb.section1_num  = read_32bit(0x04, streamFile); /* group headers? */
    sb.section2_num  = read_32bit(0x08, streamFile); /* streams headers (internal or external) */
    sb.section3_num  = read_32bit(0x0c, streamFile); /* internal streams table */
    sb.sectionX_size = read_32bit(0x10, streamFile); /* extra table, unknown (config for non-audio types) except with DSP = coefs */
    sb.flag1         = read_32bit(0x14, streamFile); /* unknown, usually -1 but can be others (0/1/2/etc) */
    sb.flag2         = read_32bit(0x18, streamFile); /* unknown, usually -1 but can be others  */

    ok = config_sb_version(&sb, streamFile);
    if (!ok) goto fail;

    sb.section1_offset = 0x1c;
    sb.section2_offset = sb.section1_offset + sb.section1_entry_size * sb.section1_num;
    sb.sectionX_offset = sb.section2_offset + sb.section2_entry_size * sb.section2_num;
    sb.section3_offset = sb.sectionX_offset + sb.sectionX_size;
    sb.sounds_offset   = sb.section3_offset + sb.section3_entry_size * sb.section3_num;

    if (!parse_sb_header(&sb, streamTest, target_stream))
        goto fail;

    close_streamfile(streamTest);

    /* CREATE VGMSTREAM */
    return init_vgmstream_ubi_sb_main(&sb, streamFile);

fail:
    close_streamfile(streamTest);
    return NULL;
}

/* .SMx - maps (sets of custom SBx files) also from Ubisoft's sound engine games in ~2000-2008+ */
VGMSTREAM * init_vgmstream_ubi_sm(STREAMFILE *streamFile) {
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    //int16_t(*read_16bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = { 0 };
    int ok, i;
    int target_stream = streamFile->stream_index;


    /* check extension (number represents the platform, see later) */
    if (!check_extensions(streamFile, "sm0,sm1,sm2,sm3,sm4,sm5,sm6,sm7,lm0,lm1,lm2,lm3,lm4,lm5,lm6,lm7"))
        goto fail;

    /* .smX (sound map) is a set of slightly different sbX files, compiled into one "map" file.
     * Map has a sbX per named area (example: menu, level1, boss1, level2...).
     * This counts subsongs from all sbX, so totals can be massive, but there are splitters into mini-smX. */


    /* PLATFORM DETECTION */
    if (!config_sb_platform(&sb, streamFile))
        goto fail;
    if (sb.big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    if (target_stream == 0) target_stream = 1;

    /* use smaller I/O buffer for performance, as this read lots of small headers all over the place */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;


    /* SM BASE HEADER */
    /* SMx layout: base header with N map area offset/sizes (some? offsets within a SBx are relative) */
    /* SBx layout: base header, section1, section2, section4, extra section, section3, data (all except base header can be null?) */
    sb.is_map = 1;
    sb.version   = read_32bit(0x00, streamFile);
    sb.map_start = read_32bit(0x04, streamFile);
    sb.map_num   = read_32bit(0x08, streamFile);

    ok = config_sb_version(&sb, streamFile);
    if (!ok || sb.map_version == 0)  goto fail;

    sb.map_entry_size = (sb.map_version < 2) ? 0x30 : 0x34;

    for (i = 0; i < sb.map_num; i++) {
        off_t offset = sb.map_start + i * sb.map_entry_size;

        /* SM AREA HEADER */
        sb.map_type     = read_32bit(offset + 0x00, streamFile); /* usually 0/1=first, 0=rest */
        sb.map_zero     = read_32bit(offset + 0x04, streamFile);
        sb.map_offset   = read_32bit(offset + 0x08, streamFile);
        sb.map_size     = read_32bit(offset + 0x0c, streamFile); /* includes sbX header, but not internal streams */
        read_string(sb.map_name, 0x20+1, offset + 0x10, streamFile); /* null-terminated and may contain garbage after null */
        if (sb.map_version >= 3)
            sb.map_unknown  = read_32bit(offset + 0x30, streamFile); /* uncommon, id/config? longer name? mem garbage? */

        /* SB HEADER */
        sb.version_empty    = read_32bit(sb.map_offset + 0x00, streamFile); /* sbX in maps don't set version */
        sb.section1_offset  = read_32bit(sb.map_offset + 0x04, streamFile) + sb.map_offset;
        sb.section1_num     = read_32bit(sb.map_offset + 0x08, streamFile);
        sb.section2_offset  = read_32bit(sb.map_offset + 0x0c, streamFile) + sb.map_offset;
        sb.section2_num     = read_32bit(sb.map_offset + 0x10, streamFile);

        if (sb.map_version < 3) {
            sb.section3_offset  = read_32bit(sb.map_offset + 0x14, streamFile) + sb.map_offset;
            sb.section3_num     = read_32bit(sb.map_offset + 0x18, streamFile);
            sb.sectionX_offset  = read_32bit(sb.map_offset + 0x1c, streamFile) + sb.map_offset;
            sb.sectionX_size    = read_32bit(sb.map_offset + 0x20, streamFile);
        } else {
            sb.section4_offset  = read_32bit(sb.map_offset + 0x14, streamFile);
            sb.section4_num     = read_32bit(sb.map_offset + 0x18, streamFile);
            sb.section3_offset  = read_32bit(sb.map_offset + 0x1c, streamFile) + sb.map_offset;
            sb.section3_num     = read_32bit(sb.map_offset + 0x20, streamFile);
            sb.sectionX_offset  = read_32bit(sb.map_offset + 0x24, streamFile) + sb.map_offset;
            sb.sectionX_size    = read_32bit(sb.map_offset + 0x28, streamFile);

            /* latest map format has another section with sounds after section 2 */
            sb.section2_num    += sb.section4_num;    /* Let's just merge it with section 2 */
            sb.sectionX_offset += sb.section4_offset; /* for some reason, this is relative to section 4 here */
        }

        VGM_ASSERT(sb.map_type != 0 && sb.map_type != 1, "UBI SM: unknown map_type %x\n", (uint32_t)offset);
        VGM_ASSERT(sb.map_zero != 0, "UBI SM: unknown map_zero %x\n", (uint32_t)offset);
        //;VGM_ASSERT(sb.map_unknown != 0, "UBI SM: unknown map_unknown at %x\n", (uint32_t)offset);
        VGM_ASSERT(sb.version_empty != 0, "UBI SM: unknown version_empty %x\n", (uint32_t)offset);

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

    /* CREATE VGMSTREAM */
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
        VGM_LOG("UBI SB: autodetecting external stream '%s'\n", sb->resource_name);

        streamData = open_streamfile_by_filename(streamFile,sb->resource_name);
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
        streamData = open_streamfile_by_filename(streamFile,sb->resource_name);
        if (!streamData) {
            VGM_LOG("UBI SB: external stream '%s' not found\n", sb->resource_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }
    //;VGM_LOG("UBI SB: stream offset=%x, size=%x, external=%i\n", (uint32_t)sb->stream_offset, sb->stream_size, sb->is_external);

    start_offset = sb->stream_offset;


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

            /* DSP extra info entry size is 0x40 (first/last 0x10 = unknown), per channel */
            dsp_read_coefs_be(vgmstream,streamFile,sb->extra_offset + 0x10, 0x40);
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
                VGM_LOG("UBI SB: skipping unknown value 0x%x before RIFF\n", read_32bitBE(start_offset+0x00,streamData));
                start_offset += 0x04;
                sb->stream_size -= 0x04;
            }

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (sb->num_samples == 0) /* sometimes not known */
                sb->num_samples = ffmpeg_data->totalSamples;
            vgmstream->num_samples = sb->num_samples;
            if (sb->num_samples != ffmpeg_data->totalSamples) {
                VGM_LOG("UBI SB: header samples differ (%i vs %i)\n", sb->num_samples, (size_t)ffmpeg_data->totalSamples);
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

            bytes = ffmpeg_make_riff_atrac3(buf, 0x100, sb->num_samples, sb->stream_size, sb->channels, sb->sample_rate, block_size, joint_stereo, encoder_delay);
            ffmpeg_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, sb->stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->num_samples = sb->num_samples;
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
            vgmstream->num_samples = sb->num_samples;
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
            vgmstream->num_samples = sb->num_samples;
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = sb->num_samples; /* ffmpeg_data->totalSamples */
            VGM_ASSERT(sb->num_samples != ffmpeg_data->totalSamples, "UBI SB: header samples differ\n");
            break;
        }

#endif
        default:
            VGM_LOG("UBI SB: unknown codec\n");
            goto fail;
    }

    strcpy(vgmstream->stream_name, sb->readable_name);

    /* open the actual for decoding (streamData can be an internal or external stream) */
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
static void parse_header_type(ubi_sb_header * sb, uint32_t header_type, off_t offset) {

    /* all types may contain memory garbage, making it harder to identify
     * usually next types can contain memory from the previous type header,
     * so if some non-audio type looks like audio it's probably repeating old data.
     * This even happens for common fields (ex. type 06 at 0x08 has prev garbage, not stream size) */

    switch(header_type) {
        case 0x01: sb->types[0x01]++; break; /* audio (all games) */
        case 0x02: sb->types[0x02]++; break; /* config? (later games) */
        case 0x03: sb->types[0x03]++; break; /* config? (later games) */
        case 0x04: sb->types[0x04]++; break; /* config? (all games) [recheck: SC:CT] */
        case 0x05: sb->types[0x05]++; break; /* config? (all games) [recheck: SC:CT] */
        case 0x06: sb->types[0x06]++; break; /* layer (later games) */
        case 0x07: sb->types[0x07]++; break; /* config? (later games) [recheck: SC:CT] */
        case 0x08: sb->types[0x08]++; break; /* config? (all games) [recheck: SC:CT] */
      //case 0x09: sb->types[0x09]++; break; /* ? */
        case 0x0a: sb->types[0x0a]++; break; /* config? (early games) */
      //case 0x0b: sb->types[0x0b]++; break; /* ? */
        case 0x0c: sb->types[0x0c]++; break; /* config? (early games) */
        case 0x0d: sb->types[0x0d]++; break; /* layer (early games) */
        case 0x0e: sb->types[0x0e]++; break; /* config? (early games) */
        case 0x0f: sb->types[0x0f]++; break; /* config? (early games) */
        default:
            VGM_LOG("UBI SB: unknown type %x at %x size %x\n", header_type, (uint32_t)offset, sb->section2_entry_size);
            break; //goto fail;
    }

    ;VGM_ASSERT(header_type == 0x06 || header_type == 0x0d,
            "UBI SB: type %x at %x size %x\n", header_type, (uint32_t)offset, sb->section2_entry_size);

    /* layer info for later
     * some values may be flags/config as multiple 0x06 can point to the same layer, with different 'flags' */

    /* 0x0d layer [Splinter Cell] */
    /* - type header:
     * 0x18: stream offset?
     * 0x20: (sample rate * layers) + 1?
     * 0x24: layers/channels?
     * 0x30: external flag
     * 0x34: external name
     * 0x5C: stream offset
     * 0x64: stream size (not including padding)
     * 0x78/7c: codec?
     *
     * - layer header at stream_offset:
     * 0x00: version? (0x02000000)
     * 0x04: layers
     * 0x08: stream size (not including padding)
     * 0x0c: size?
     * 0x10: count?
     * 0x14: min block size?
     *
     * - blocked data (unlike other layers, first block data is standard Ubi IMA headers, v3):
     * 0x00: block number (from 0x01 to block_count)
     * 0x04: current offset (within stream_offset)
     * - per layer:
     * 0x00: layer data size (varies between blocks, and one layer may have more than other, even the header)
     */

    /* Rainbow Six 3 */ //todo also check layer header
    /* Prince of Persia: Sands of Time (all) 0x000A0004 */
    /* Batman: Rise of Sin Tzu (2003)(GC)-map - 0x000A0002 */
    /* - type header (bizarrely but thankfully doesn't change between platforms):
     * 0x1c: sample rate * layers
     * 0x20: layers/channels?
     * 0x2c: external flag?
     * 0x30: external name
     * 0x58: stream offset
     * 0x5c: original rate * layers?
     * 0x60: stream size (not including padding)
     * 0x64: number of samples
     *
     * - layer header at stream_offset (BE on GC):
     * 0x00: version? (0x04)
     * 0x04: layers
     * 0x08: stream size (not including padding)
     * 0x0c: blocks count
     * 0x10: block header size
     * 0x14: block size
     * 0x18: ?
     * 0x1c: size of next section
     * - per layer:
     * 0x00: layer header size
     * codec header per layer
     * 0x00~0x20: standard Ubi IMA header (version 0x05, LE)
     *
     * - blocked data:
     * 0x00: block number (from 0x01 to block_count)
     * 0x04: current offset (within stream_offset)
     * 0x08: always 0x03
     * - per layer:
     * 0x00: layer data size (varies between blocks, and one layer may have more than other)
     */

    /* Splinter Cell: Essentials (PSP)-map 0x0012000C */
    /* - type header:
     * 0x08: header extra offset
     * 0x1c: layers
     * 0x28: external flag?
     * 0x2c: external flag?
     * 0x30: stream name
     * 0x5c: config?
     * 0x60: stream size
     * 0x64: stream offset?
     *
     * - in header extra offset
     * 0x00: sample rate
     * 0x04: 16?
     * 0x08: channels?
     * 0x0c: codec?
     *
     * - layer header at stream_offset:
     * 0x00: version? (0x07000000)
     * 0x04: 0x03?
     * 0x08: layers/channels?
     * 0x0c: stream size
     * 0x10: blocks count
     * 0x14: block header size
     * 0x18: block size
     * - per layer:
     * 0x00: approximate layer data size per block
     * - per layer
     * 0x00~0x0c: weird header thing? -1/0/0c...
     *
     * - blocked data:
     * 0x00: block number (from 0x01 to block_count)
     * 0x04: current offset (within stream_offset)
     * 0x08: always 0x03
     * - per layer:
     * 0x00: layer data size (varies between blocks, and one layer may have more than other)
     */


    /* 0x06 layer [TMNT (PS2)-bank] */
    /* - type header:
     * 0x08: header extra offset
     * 0x1c: external flag?
     * 0x20: layers/channels?
     * 0x24: layers/channels?
     * 0x28: config? same for all
     * 0x2c: stream size
     * 0x30: stream offset
     * 0x38: name extra offset
     *
     * - in header extra offset
     * 0x00: sample rate
     * 0x04: channels?
     * 0x08: codec?
     *
     * - layer header at stream_offset:
     * 0x00: version? (0x08000B00)
     * 0x04: config? (0x0e/0x0b/etc)
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

    /* Splinter Cell 3D (2011)(3DS)-map 0x00130001 */ //todo

    /* Open Season (2005)(PS2)-map 0x00180003 */
    /* Rainbow Six Vegas (2007)(PSP)-bank 0x00180006 */
    /* Star Wars - Lethal Alliance (2006)(PSP)-map 0x00180007 */
    /* - type header:
     * 0x0c: header extra offset
     * 0x20: layers
     * 0x2c: stream size
     * 0x30: stream offset
     * 0x38: name extra offset
     *
     * - in header extra offset
     * 0x00: sample rate
     * 0x04: 16?
     * 0x08: channels?
     * 0x0c: codec (03=Ubi, 01=PCM16LE in SW:LA)
     *
     * - layer header at stream_offset:
     * - blocked data:
     * same as TMNT PSP-map
     */

    /* TMNT (2007)(PSP)-map 0x00190001 */
    /* - type header:
     * 0x0c: header extra offset
     * 0x20: layers
     * 0x24: total channels?
     * 0x28: config?
     * 0x2c: stream size
     * 0x30: stream offset
     * 0x38: name extra offset
     *
     * - in header extra offset
     * 0x00: sample rate
     * 0x04: channels?
     * 0x08: codec?
     *
     * - layer header at stream_offset:
     * - blocked data:
     * same as TMNT PS2-bank, but codec header size is 0
     */

    //todo Surf's Up (PC?)-map

    /* Splinter Cell Classic Trilogy HD (2011)(PS3)-map 0x001d0000 */
    /* - type header:
     * 0x0c: header extra offset
     * 0x20: layers
     * 0x44: stream size
     * 0x48: stream offset
     * 0x54: name extra offset
     *
     * - in header extra offset
     * 0x00: sample rate
     * 0x04: channels?
     * 0x08: codec?
     *
     * - layer header at stream_offset:
     * - blocked data:
     * same as TMNT PS2-bank, but codec header size is 0
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
        uint32_t header_type;

        header_type = read_32bit(offset + 0x04, streamFile);
        parse_header_type(sb, header_type, offset);

        /* ignore non-audio entries */
        if (header_type != 0x01)
            continue;

        /* weird case when there is no internal substream ID and just seem to rotate every time type changes, joy */
        if (sb->has_rotating_ids) { /* assumes certain configs can't happen in this case */
            int current_is_external = 0;
            int type = read_32bit(offset + sb->cfg_stream_type, streamFile);

            if (sb->cfg_external_flag) {
                current_is_external = read_32bit(offset + sb->cfg_external_flag, streamFile);
            } else if (sb->has_extra_name_flag && read_32bit(offset + sb->cfg_extra_name, streamFile) != 0xFFFFFFFF) {
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

        /* parse audio entry based on config */
        sb->header_index    = i;
        sb->header_offset   = offset;

        sb->header_id       = read_32bit(offset + 0x00, streamFile);
        sb->header_type     = read_32bit(offset + 0x04, streamFile);
        sb->stream_size     = read_32bit(offset + sb->cfg_stream_size, streamFile);
        sb->extra_offset    = read_32bit(offset + sb->cfg_extra_offset, streamFile) + sb->sectionX_offset;
        sb->stream_offset   = read_32bit(offset + sb->cfg_stream_offset, streamFile);
        sb->channels        = (sb->has_short_channels) ?
                    (uint16_t)read_16bit(offset + sb->cfg_channels, streamFile) :
                    (uint32_t)read_32bit(offset + sb->cfg_channels, streamFile);
        sb->sample_rate     = read_32bit(offset + sb->cfg_sample_rate, streamFile);
        sb->stream_type     = read_32bit(offset + sb->cfg_stream_type, streamFile);

        /* some games may store number of samples at different locations */
        if (sb->cfg_samples_flag && read_32bit(offset + sb->cfg_samples_flag, streamFile) != 0) {
            sb->num_samples = read_32bit(offset + sb->cfg_num_samples2, streamFile);
        } else if (sb->cfg_samples_bitflag && (read_32bit(offset + sb->cfg_samples_bitflag, streamFile) & 0x10)) {
            sb->num_samples = read_32bit(offset + sb->cfg_num_samples2, streamFile);
            VGM_ASSERT(sb->num_samples == 0, "UBI SB: bad bitflag found\n");
        } else if (sb->cfg_num_samples) {
            sb->num_samples = read_32bit(offset + sb->cfg_num_samples, streamFile);
        }

        if (sb->has_rotating_ids) {
            sb->stream_id   = current_id;
        } else if (sb->cfg_stream_id) {
            sb->stream_id   = read_32bit(offset + sb->cfg_stream_id, streamFile);
        }

        /* external stream name can be found in the header (first versions) or the sectionX table (later versions) */
        if (sb->cfg_stream_name) {
            read_string(sb->resource_name, sb->resource_name_size, offset + sb->cfg_stream_name, streamFile);
        } else {
            sb->cfg_stream_name = read_32bit(offset + sb->cfg_extra_name, streamFile);
            read_string(sb->resource_name, sb->resource_name_size, sb->sectionX_offset + sb->cfg_stream_name, streamFile);
        }

        /* external flag is not always set and must be derived */
        if (sb->cfg_external_flag) {
            sb->is_external = read_32bit(offset + sb->cfg_external_flag, streamFile);
        } else if (sb->has_extra_name_flag && read_32bit(offset + sb->cfg_extra_name, streamFile) != 0xFFFFFFFF) {
            sb->is_external = 1; /* -1 in extra_name means internal */
        } else if (sb->section3_num == 0) {
            sb->is_external = 1;
        } else {
            sb->autodetect_external = 1; /* let the parser guess later */

            if (sb->resource_name[0] == '\0')
                sb->autodetect_external = 0; /* no name */
            if (sb->sectionX_size > 0 && sb->cfg_stream_name > sb->sectionX_size)
                sb->autodetect_external = 0; /* name outside extra table == is internal */
        }

        //todo check sb->has_internal_names in case of garbage
        /* build a full name for stream */
        if (sb->is_map) {
            if (sb->resource_name[0]) {
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s/%d/%08x/%s", sb->map_name, bank_streams, sb->header_id, sb->resource_name);
            } else {
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s/%d/%08x", sb->map_name, bank_streams, sb->header_id);
            }
        } else {
            if (sb->resource_name[0]) {
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%08x/%s", sb->header_id, sb->resource_name);
            } else {
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s", sb->resource_name);
            }
        }

    }

    //;VGM_LOG("UBI SB: types "); for (int i = 0; i < 16; i++) { VGM_ASSERT(sb->types[i], "%02x=%i ",i,sb->types[i]); } VGM_LOG("\n");


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

    if (!(sb->cfg_stream_id || sb->has_rotating_ids || sb->is_map) && sb->section3_num > 1) {
        VGM_LOG("UBI SB: unexpected number of internal stream groups %i\n", sb->section3_num);
        goto fail;
    }

    //;VGM_LOG("UBI SB: target at %x (cfg %x), extra=%x, name=%s\n", (uint32_t)sb->header_offset, sb->section2_entry_size, (uint32_t)sb->extra_offset, sb->resource_name);


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
                case UBI_3DS:
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

        case 0x06: /* PS ADPCM (later PSP and PS3(?) games) */
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
        sb->xma_header_offset = read_32bit(sb->header_offset + sb->cfg_xma_offset, streamFile) + sb->sectionX_offset;
    }

    /* uncommon but possible */
    //VGM_ASSERT(sb->is_external && sb->section3_num != 0, "UBI SS: mixed external and internal streams\n");

    /* seems that can be safely ignored */
    //VGM_ASSERT(sb->is_external && sb->cfg_stream_id && sb->stream_id > 0, "UBI SB: unexpected external stream with stream id\n");

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
                    int index = read_32bit(table_offset + 0x08 * j + 0x00, streamFile) & 0x0000FFFF;

                    if (index == sb->header_index) {
                        if (!(sb->cfg_stream_id || sb->has_rotating_ids) && table2_num > 1) {
                            VGM_LOG("UBI SB: unexpected number of internal stream map groups %i\n", table2_num);
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

            if ((sb->cfg_stream_id || sb->has_rotating_ids) && sb->section3_num > 1) {
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

static int config_sb_platform(ubi_sb_header * sb, STREAMFILE *streamFile) {
    char filename[PATH_LIMIT];
    int filename_len;
    char platform_char;
    uint32_t version;

    /* to find out hijacking platforms */
    version = read_32bitLE(0x00, streamFile);

    /* get X from .sbX/smX */
    get_streamfile_name(streamFile,filename,sizeof(filename));
    filename_len = strlen(filename);
    platform_char = filename[filename_len - 1];

    switch(platform_char) {
        case '0':
            sb->platform = UBI_PC;
            break;
        case '1':
            sb->platform = UBI_PS2;
            break;
        case '2':
            sb->platform = UBI_XBOX;
            break;
        case '3':
            sb->platform = UBI_GC;
            break;
        case '4':
            switch(version) { /* early PSP clashes with X360 */
                case 0x0012000C: /* multiple games use this ID and all are sb4/sm4 */
                    sb->platform = UBI_PSP;
                    break;
                default:
                    sb->platform = UBI_X360;
                    break;
            }
            break;
        case '5':
            switch(version) { /* 3DS could be sb8/sm8 but somehow hijacks extension */
                case 0x00130001: /* Splinter Cell 3DS (2011) */
                    sb->platform = UBI_3DS;
                    break;
                default:
                    sb->platform = UBI_PSP;
                    break;
            }
            break;
        case '6':
            sb->platform = UBI_PS3;
            break;
        case '7':
            sb->platform = UBI_WII;
            break;
        default:
            goto fail;
    }

    sb->big_endian =
            sb->platform == UBI_GC ||
            sb->platform == UBI_PS3 ||
            sb->platform == UBI_X360 ||
            sb->platform == UBI_WII;


    return 1;
fail:
    return 0;
}

static int config_sb_version(ubi_sb_header * sb, STREAMFILE *streamFile) {
    int is_biadd_psp = 0;

    /* The type 1 audio header varies with almost every game + platform (some kind of class serialization?),
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

    //todo set samples flag where "varies"

    /* common */
    sb->section3_entry_size = 0x08;
    sb->resource_name_size  = 0x24; /* maybe 0x28 or 0x20 for some but ok enough (null terminated) */
    /* this is same in all games since ~2003 */
    sb->cfg_stream_size     = 0x08;
    sb->cfg_extra_offset    = 0x0c;
    sb->cfg_stream_offset   = 0x10;


    /* Batman: Vengeance (2001)(PS2)-map 0x00000003 */
    /* Batman: Vengeance (2001)(GC)-map 0x00000003 */
    /* Disney's Tarzan: Untamed (2001)(GC)-map 0x00000003 */

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

        sb->cfg_stream_size     = 0x0c;
        sb->cfg_extra_offset    = 0x10;
        sb->cfg_stream_offset   = 0x14;

        sb->cfg_external_flag   = 0x30;
        sb->cfg_stream_id       = 0x34;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_sample_rate     = 0x50;
        sb->cfg_channels        = 0x56;
        sb->cfg_stream_type     = 0x58;
        sb->cfg_stream_name     = 0x5c;

        sb->has_short_channels = 1;
        return 1;
    }
#endif
#if 0
    /* Splinter Cell (2002)(PC)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(PC)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_PC) {
        /* Stream types:
         * 0x01: PCM
         * 0x02: unsupported codec, appears to be Ubi IMA in a blocked layout
         * 0x04: Ubi IMA v3 (not Vorbis)
         */
        sb->section1_entry_size = 0x58;
        sb->section2_entry_size = 0x80;

        sb->map_version = 1;

        sb->cfg_stream_size     = 0x0c;
        sb->cfg_extra_offset    = 0x10;
        sb->cfg_stream_offset   = 0x14;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_stream_id       = 0x2c;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4a;
        sb->cfg_stream_type     = 0x4c;
        sb->cfg_stream_name     = 0x50;

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

        sb->cfg_external_flag   = 0x24; /* maybe 0x28 */
        sb->cfg_num_samples     = 0x30;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4a;
        sb->cfg_stream_type     = 0x4c;
        sb->cfg_stream_name     = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        //has layer 0d (main game)
        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(PS2)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PS2) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_PS2)) { /* main game */
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag   = 0; /* no apparent flag */
        sb->cfg_channels        = 0x20;
        sb->cfg_sample_rate     = 0x24;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_stream_name     = 0x40;
        sb->cfg_stream_type     = 0x68;

        //has layer 0d (main game)
        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(Xbox)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_XBOX) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_XBOX)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x78;

        sb->cfg_external_flag   = 0x24; /* maybe 0x28 */
        sb->cfg_num_samples     = 0x30;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4a;
        sb->cfg_stream_type     = 0x4c; /* may contain garbage */
        sb->cfg_stream_name     = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        //has layer 0d (main game)
        return 1;
    }


    //todo Batman offsets are slightly off for some subsongs, or maybe get external wrong (ex. 222~226)
    /* Batman: Rise of Sin Tzu (2003)(GC)-map - 0x000A0002 */
    /* Prince of Persia: Sands of Time (2003)(GC)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_GC) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_GC)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x74;

        sb->map_version = 2;

        sb->cfg_external_flag   = 0x20; /* maybe 0x24 */
        sb->cfg_num_samples     = 0x2c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_channels        = 0x46;
        sb->cfg_stream_type     = 0x48;
        sb->cfg_stream_name     = 0x4c;

        sb->has_short_channels = 1;
        //has layer 0d (POP:SOT main game, Batman)
        return 1;
    }

    /* Tom Clancy's Rainbow Six 3 (2003)(PS2)-bank */
    if (sb->version == 0x000A0007 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag   = 0; /* no apparent flag */
        sb->cfg_channels        = 0x20;
        sb->cfg_sample_rate     = 0x24;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_stream_name     = 0x40;
        sb->cfg_stream_type     = 0x68;

        //has layer 0d
        return 1;
    }

    /* Myst IV Demo (2004)(PC)-bank */
    if (sb->version == 0x00100000 && sb->platform == UBI_PC) { /* final game is different */
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0xa4;

        sb->cfg_external_flag   = 0x24;
        sb->cfg_num_samples     = 0x34;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4c;
        sb->cfg_stream_type     = 0x50;
        sb->cfg_stream_name     = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(PC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x84;

        sb->cfg_external_flag   = 0x24;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4c;
        sb->cfg_stream_type     = 0x50;
        sb->cfg_stream_name     = 0x54;

        sb->has_internal_names = 1;
        //no layers
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(PS2)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag = 0; /* no apparent flag */
        sb->cfg_channels        = 0x20;
        sb->cfg_sample_rate     = 0x24;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_stream_name     = 0x40;
        sb->cfg_stream_type     = 0x68;

        //no layers
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(Xbox)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x90;

        sb->cfg_external_flag   = 0x24;
        sb->cfg_num_samples     = 0x44;
        sb->cfg_sample_rate     = 0x58;
        sb->cfg_channels        = 0x60;
        sb->cfg_stream_type     = 0x64; /* may contain garbage */
        sb->cfg_stream_name     = 0x68;

        sb->has_internal_names = 1;
        //no layers
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(GC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x78;

        sb->cfg_external_flag   = 0x20;
        sb->cfg_num_samples     = 0x2c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_channels        = 0x48;
        sb->cfg_stream_type     = 0x4c;
        sb->cfg_stream_name     = 0x50;

        //no layers
        return 1;
    }

    /* two configs with same id; use project file as identifier */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "BIAAUDIO.SP4");
        if (streamTest) {
            is_biadd_psp = 1;
            close_streamfile(streamTest);
        }
    }

    /* Prince of Persia: Revelations (2005)(PSP)-bank */
    /* Splinter Cell: Essentials (2006)(PSP)-map */
    /* Beowulf - The Game (2007)(PSP)-map */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && !is_biadd_psp) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x84;

        sb->map_version = 2;

        sb->cfg_external_flag   = 0x24;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4c;
        sb->cfg_stream_type     = 0x50;
        sb->cfg_stream_name     = 0x54;

        sb->has_internal_names = 1;
        //has layers 06 (SC:E only)

        //todo Beowulf needs rotating ids, but fails in some subsong offsets
        // (ex. subsong 33415 should point to AT3 but offset is a VAGp)
        return 1;
    }

    /* Brothers in Arms - D-Day (2006)(PSP)-bank */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && is_biadd_psp) {
        sb->section1_entry_size = 0x80;
        sb->section2_entry_size = 0x94;

        sb->cfg_external_flag   = 0x24;
        sb->cfg_samples_flag    = 0x28;
        sb->cfg_stream_id       = 0x2c;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_num_samples2    = 0x38;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4c;
        sb->cfg_stream_type     = 0x50;
        sb->cfg_stream_name     = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

#if 0
    /* Splinter Cell 3D (2011)(3DS)-map */
    if (sb->version == 0x00130001 && sb->platform == UBI_3DS) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x4c;

        sb->map_version = 2;

        sb->cfg_external_flag   = 0;
        //sb->cfg_stream_id       = 0;
        sb->cfg_num_samples     = 0x1c;
        sb->cfg_sample_rate     = 0x30;
        sb->cfg_channels        = 0x38;
        sb->cfg_stream_type     = 0x3c;
        sb->cfg_extra_name      = 0x40;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1; //todo?
        //has layer 06

        //todo SC3DS codecs:
        // 00: RAW_PCM
        // 01: FMT_CWAV
        // 03: Ubi IMA
        return 1;
    }
#endif

    /* Prince of Persia: The Two Thrones (2005)(PC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x78;

        sb->cfg_external_flag   = 0x2c;
        sb->cfg_stream_id       = 0x34;
        sb->cfg_num_samples     = 0x40;
        sb->cfg_sample_rate     = 0x54;
        sb->cfg_channels        = 0x5c;
        sb->cfg_stream_type     = 0x60;
        sb->cfg_extra_name      = 0x64;

        sb->has_extra_name_flag = 1;
        //no layers
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PS2)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x5c;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x2c;
        sb->cfg_sample_rate     = 0x30;
        sb->cfg_num_samples     = 0x3c;
        sb->cfg_extra_name      = 0x4c;
        sb->cfg_stream_type     = 0x50;

        sb->has_extra_name_flag = 1;
        //no layers
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(Xbox)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->cfg_external_flag   = 0;
        sb->cfg_stream_id       = 0;
        sb->cfg_num_samples     = 0x28;
        sb->cfg_sample_rate     = 0x3c;
        sb->cfg_channels        = 0x44;
        sb->cfg_stream_type     = 0x48;
        sb->cfg_extra_name      = 0x4c;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1;
        //no layers
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(GC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag   = 0x28; /* maybe 0x2c */
        sb->cfg_num_samples     = 0x3c;
        sb->cfg_sample_rate     = 0x50;
        sb->cfg_channels        = 0x58;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x60;

        //no layers
        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(PC)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x60;

        sb->map_version = 2;

        sb->cfg_external_flag   = 0x24;
        sb->cfg_num_samples     = 0x30;
        sb->cfg_sample_rate     = 0x44;
        sb->cfg_channels        = 0x4c;
        sb->cfg_stream_type     = 0x50;
        sb->cfg_extra_name      = 0x54;

        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(Xbox)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x4c;

        sb->map_version = 2;

        sb->cfg_external_flag   = 0;
        sb->cfg_stream_id       = 0;
        sb->cfg_num_samples     = 0x18;
        sb->cfg_sample_rate     = 0x30;
        sb->cfg_channels        = 0x38;
        sb->cfg_stream_type     = 0x3c;
        sb->cfg_extra_name      = 0x40;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1;
        return 1;
    }

#if 0
    /* Far cry: Instincts - Evolution (2006)(Xbox)-bank */
    if (sb->version == 0x00170000 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag   = 0;
        sb->cfg_stream_id       = 0;
        sb->cfg_num_samples     = 0x28;
        sb->cfg_sample_rate     = 0x3c;
        sb->cfg_channels        = 0x44;
        sb->cfg_stream_type     = 0x48;
        sb->cfg_extra_name      = 0x58;

        return 1;
    }
#endif

    /* Red Steel (2006)(Wii)-bank */
    if (sb->version == 0x00180006 && sb->platform == UBI_WII) { /* same as 0x00150000 */
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag   = 0x28; /* maybe 0x2c */
        sb->cfg_num_samples     = 0x3c;
        sb->cfg_sample_rate     = 0x50;
        sb->cfg_channels        = 0x58;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x60;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(PC)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x7c;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0x2c;
        sb->cfg_stream_id       = 0x34;
        sb->cfg_channels        = 0x5c;
        sb->cfg_sample_rate     = 0x54;
        sb->cfg_num_samples     = 0x40;
        sb->cfg_num_samples2    = 0x48;
        sb->cfg_stream_type     = 0x60;
        sb->cfg_extra_name      = 0x64;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(X360)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x78;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0x2c;
        sb->cfg_stream_id       = 0x30;
        sb->cfg_samples_flag    = 0x34;
        sb->cfg_channels        = 0x5c;
        sb->cfg_sample_rate     = 0x54;
        sb->cfg_num_samples     = 0x40;
        sb->cfg_num_samples2    = 0x48;
        sb->cfg_stream_type     = 0x60;
        sb->cfg_extra_name      = 0x64;
        sb->cfg_xma_offset      = 0x70;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(Xbox)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0;
        sb->cfg_num_samples     = 0x28;
        sb->cfg_stream_id       = 0;
        sb->cfg_sample_rate     = 0x3c;
        sb->cfg_channels        = 0x44;
        sb->cfg_stream_type     = 0x48;
        sb->cfg_extra_name      = 0x4c;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1;
        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(GC)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_stream_id       = 0x2c;
        sb->cfg_num_samples     = 0x3c;
        sb->cfg_sample_rate     = 0x50;
        sb->cfg_channels        = 0x58;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x60;

        return 1;
    }

    /* Open Season (2005)(PS2)-map - 0x00180003 */
    /* Open Season (2005)(PSP)-map - 0x00180003 */
    /* Star Wars - Lethal Alliance (2006)(PSP)-map - 0x00180007 */
    if ((sb->version == 0x00180003 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00180003 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180007 && sb->platform == UBI_PSP)) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0;
        sb->cfg_samples_bitflag = 0x20;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x2c;
        sb->cfg_num_samples     = 0x34;
        sb->cfg_num_samples     = 0x3c;
        sb->cfg_extra_name      = 0x44;
        sb->cfg_stream_type     = 0x48;

        sb->has_extra_name_flag = 1;
        //has layer 06
        return 1;
    }

    /* Prince of Persia: Rival Swords (2007)(PSP)-bank */
    if (sb->version == 0x00180005 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x2c;
        //sb->cfg_num_samples   = 0x34 or 0x3c /* varies */
        sb->cfg_extra_name      = 0x44;
        sb->cfg_stream_type     = 0x48;

        sb->has_extra_name_flag = 1;
        return 1;
    }

#if 0 //todo join with other 0x0018000x configs?
    /* Rainbow Six Vegas (2007)(PSP)-bank */
    if (sb->version == 0x00180006 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x2c;
        //sb->cfg_num_samples   = 0x34 or 0x3c /* varies */
        sb->cfg_extra_name      = 0x44;
        sb->cfg_stream_type     = 0x48;

        sb->has_extra_name_flag = 1;
        sb->has_rotating_ids = 1; //???
        return 1;
    }
#endif

    /* Prince of Persia: Rival Swords (2007)(Wii)-bank */
    if (sb->version == 0x00190003 && sb->platform == UBI_WII) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->cfg_external_flag   = 0x28; /* maybe 0x2c */
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_extra_name      = 0x58;
        sb->cfg_stream_type     = 0x5c;

        return 1;
    }
    
    /* TMNT (2007)(PC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_stream_id       = 0x2c;
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x58;

        return 1;
    }

    /* TMNT (2007)(PS2)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x5c;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x2c;
        //sb->cfg_num_samples   = 0x34 or 0x3c /* varies */
        sb->cfg_extra_name      = 0x44;
        sb->cfg_stream_type     = 0x48;

        sb->has_extra_name_flag = 1;
        //has layer 06
        return 1;
    }

    /* TMNT (2007)(GC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_GC) { /* same as 0x00190003 */
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->cfg_external_flag   = 0x28; /* maybe 0x2c */
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x58;

        return 1;
    }

     /* TMNT (2007)(X360)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_samples_flag    = 0x30;
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_num_samples2    = 0x50;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x58;
        sb->cfg_xma_offset      = 0x6c;

        return 1;
    }

    /* TMNT (2007)(PSP)-map */
    if (sb->version == 0x00190001 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x2c;
        sb->cfg_num_samples     = 0x34;
        sb->cfg_stream_type     = 0x48;
        sb->cfg_extra_name      = 0x44;

        sb->has_extra_name_flag = 1;
        //has layer 06
        return 1;
    }

    /* Surf's Up (2007)(PC)-bank */
    if (sb->version == 0x00190005 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->cfg_external_flag   = 0x28; /* maybe 0x2c */
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x58;

        return 1;
    }

    /* Surf's Up (2007)(PS3)-bank */
    /* Splinter Cell: Double Agent (2007)(PS3)-map */
    if (sb->version == 0x00190005 && sb->platform == UBI_PS3) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_stream_id       = 0x2c;
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x58;

        return 1;
    }

    /* Surf's Up (2007)(X360)-bank */
    if (sb->version == 0x00190005 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_samples_flag    = 0x30;
        sb->cfg_channels        = 0x3c;
        sb->cfg_sample_rate     = 0x40;
        sb->cfg_num_samples     = 0x48;
        sb->cfg_num_samples2    = 0x50;
        sb->cfg_stream_type     = 0x5c;
        sb->cfg_extra_name      = 0x58;
        sb->cfg_xma_offset      = 0x6c;

        return 1;
    }

    /* Surf's Up (2007)(PSP)-map */
    if (sb->version == 0x00190005 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x2c;
        sb->cfg_num_samples     = 0x34;
        sb->cfg_stream_type     = 0x48;
        sb->cfg_extra_name      = 0x44;

        sb->has_extra_name_flag = 1;
        //no layers
        return 1;
    }

    /* Michael Jackson: The Experience (2010)(PSP)-map */
    if (sb->version == 0x001d0000 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x40;
        sb->section2_entry_size = 0x60;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0;
        sb->cfg_channels        = 0x28;
        sb->cfg_sample_rate     = 0x30;
        sb->cfg_num_samples     = 0x38;
        sb->cfg_extra_name      = 0x48;
        sb->cfg_stream_type     = 0x4c; /* 06|08 */

        sb->has_extra_name_flag = 1;
        //no layers
        return 1;
    }

    /* Splinter Cell Classic Trilogy HD (2011)(PS3)-map */
    if (sb->version == 0x001d0000 && sb->platform == UBI_PS3) {
        sb->section1_entry_size = 0x5c;
        sb->section2_entry_size = 0x80;

        sb->map_version = 3;

        sb->cfg_external_flag   = 0x28;
        sb->cfg_stream_id       = 0x30;
        sb->cfg_samples_flag    = 0x34;
        sb->cfg_channels        = 0x44;
        sb->cfg_sample_rate     = 0x4c;
        sb->cfg_num_samples     = 0x54;
        sb->cfg_num_samples2    = 0x5c;
        sb->cfg_stream_type     = 0x68;
        sb->cfg_extra_name      = 0x64;

        //has layer 06
        return 1;
    }

    VGM_LOG("UBI SB: unknown SB/SM version+platform for %08x\n", sb->version);
    return 0;
}
