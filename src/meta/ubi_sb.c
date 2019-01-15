#include "meta.h"
#include "../coding/coding.h"


typedef enum { UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_DSP, RAW_XBOX, FMT_VAG, FMT_AT3, RAW_AT3, FMT_XMA1, RAW_XMA1, FMT_OGG, FMT_CWAV } ubi_sb_codec;
typedef enum { UBI_PC, UBI_PS2, UBI_XBOX, UBI_GC, UBI_X360, UBI_PSP, UBI_PS3, UBI_WII, UBI_3DS } ubi_sb_platform;
typedef struct {
    ubi_sb_platform platform;
    int big_endian;
    int total_subsongs;
    int is_external;
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

    /* SB info (some values are derived depending if it's standard sbX or map sbX) */
    int is_map;
    uint32_t version;           /* 16b+16b major+minor version */
    uint32_t version_empty;     /* map sbX versions are empty */
    /* events? (often share header_id/type with some descriptors,
     * but may exists without headers or header exist without this) */
    size_t section1_num;
    size_t section1_offset;
    /* descriptors, audio header or other config types */
    size_t section2_num;
    size_t section2_offset;
    /* internal streams table (id and offset), referenced by each header */
    size_t section3_num;
    size_t section3_offset;
    /* section with sounds in some map versions */
    size_t section4_num;
    size_t section4_offset;
    /* extra table, config for certain types (DSP coefs, external resources, layer headers, etc) */
    size_t sectionX_size;
    size_t sectionX_offset;
    /* unknown, usually -1 but can be others (0/1/2/etc) */
    int flag1;
    int flag2;
    /* where sound data starts, derived */
    size_t sounds_offset;


    /* header/stream info config */
    /* audio header varies slightly per game/version but not enough parse case by case,
     * instead we configure sizes and offsets to where each variable is */
    int map_version;                /* represents map style (1=first, 2=mid, 3=latest) */
    size_t section1_entry_size;
    size_t section2_entry_size;
    size_t section3_entry_size;
    size_t resource_name_size;
    /* type 0x01 (sample) config */
    off_t  cfga_stream_size;
    off_t  cfga_stream_offset;
    off_t  cfga_extra_offset;
    off_t  cfga_group_id;
    off_t  cfga_stream_type;

    off_t  cfga_external_flag;   /* stream is external */
    off_t  cfga_loop_flag;       /* stream loops */
    off_t  cfga_num_samples;     /* num_samples/loop start */
    off_t  cfga_num_samples2;    /* num_samples/loop end (if loop set) */
    off_t  cfga_sample_rate;
    off_t  cfga_channels;
    off_t  cfga_stream_name;     /* where the resource name is within the header */
    off_t  cfga_extra_name;      /* where the resource name is within sectionX */
    off_t  cfga_xma_offset;
    int and_external_flag;      /* value for some flags can be int or bitflags */
    int and_loop_flag;
    int and_group_id;
    int shr_group_id;
    int has_full_loop;          /* loop flag means full loop */
    int has_short_channels;     /* channels value can be 16b or 32b */
    int has_internal_names;     /* resource name doubles as internal name in earlier games, or may contain garbage */
    /* type 0x05/0c (sequence?) config */
    off_t cfgs_extra_offset;
    off_t cfgs_sequence_count;
    /* type 0x06/0d (multilayer) config */
    //off_t cfgl_extra_offset;


    /* header/stream info */
    uint32_t header_id;         /* 16b+16b group+sound id identifier (unique within a sbX, but not smX) */
    uint32_t header_type;       /* audio type (we only need 'standard audio' or 'layered audio') */
    size_t stream_size;         /* size of the audio data */
    off_t stream_offset;        /* offset within the data section (internal) or absolute (external) to the audio */
    off_t extra_offset;         /* offset within sectionX to extra data */
    uint32_t stream_type;       /* rough codec value */

    uint32_t group_id;          /* internal id to reference in section3 */
    //int sequence_count;         /* number of segments in a sequence type */

    int loop_flag;
    int loop_start;             /* loop starts that aren't 0 do exist but are very rare (ex. Beowulf PSP #33407) */
    int num_samples;            /* should match manually calculated samples */
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
static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile, int target_subsong);
static int config_sb_platform(ubi_sb_header * sb, STREAMFILE *streamFile);
static int config_sb_version(ubi_sb_header * sb, STREAMFILE *streamFile);


/* .SBx - banks from Ubisoft's DARE (Digital Audio Rendering Engine) engine games in ~2000-2008+ */
VGMSTREAM * init_vgmstream_ubi_sb(STREAMFILE *streamFile) {
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = { 0 };
    int ok;
    int target_subsong = streamFile->stream_index;

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

    if (target_subsong == 0) target_subsong = 1;

    /* use smaller I/O buffer for performance, as this read lots of small headers all over the place */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;


    /* SB HEADER */
    /* SBx layout: base header, section1, section2, extra section, section3, data (all except base header can be null) */
    sb.is_map = 0;
    sb.version       = read_32bit(0x00, streamFile);
    sb.section1_num  = read_32bit(0x04, streamFile);
    sb.section2_num  = read_32bit(0x08, streamFile);
    sb.section3_num  = read_32bit(0x0c, streamFile);
    sb.sectionX_size = read_32bit(0x10, streamFile);
    sb.flag1         = read_32bit(0x14, streamFile);
    sb.flag2         = read_32bit(0x18, streamFile);

    ok = config_sb_version(&sb, streamFile);
    if (!ok) goto fail;

    sb.section1_offset = 0x1c;
    sb.section2_offset = sb.section1_offset + sb.section1_entry_size * sb.section1_num;
    sb.sectionX_offset = sb.section2_offset + sb.section2_entry_size * sb.section2_num;
    sb.section3_offset = sb.sectionX_offset + sb.sectionX_size;
    sb.sounds_offset   = sb.section3_offset + sb.section3_entry_size * sb.section3_num;

    if (!parse_sb_header(&sb, streamTest, target_subsong))
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
    int target_subsong = streamFile->stream_index;


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

    if (target_subsong == 0) target_subsong = 1;

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

        if (!parse_sb_header(&sb, streamTest, target_subsong))
            goto fail;
    }

    if (sb.total_subsongs == 0) {
        VGM_LOG("UBI SB: no subsongs\n");
        goto fail;
    }

    if (target_subsong < 0 || target_subsong > sb.total_subsongs) {
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


    /* open external stream if needed */
    if (sb->is_external) {
        streamData = open_streamfile_by_filename(streamFile,sb->resource_name);
        if (!streamData) {
            VGM_LOG("UBI SB: external stream '%s' not found\n", sb->resource_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }
    ;VGM_LOG("UBI SB: stream offset=%x, size=%x, external=%i\n", (uint32_t)sb->stream_offset, sb->stream_size, sb->is_external);

    start_offset = sb->stream_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sb->channels,sb->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->sample_rate = sb->sample_rate;
    vgmstream->num_streams = sb->total_subsongs;
    vgmstream->stream_size = sb->stream_size;

    vgmstream->num_samples = sb->num_samples;
    vgmstream->loop_start_sample = sb->loop_start;
    vgmstream->loop_end_sample = sb->num_samples;

    switch(sb->codec) {
        case UBI_ADPCM:
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE even on Wii */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (sb->stream_type == 0x00) ? sb->stream_size / sb->channels : 0x10; /* TODO: needs testing */
            if (vgmstream->num_samples == 0) { /* early PS2 games may not set it for a few internal streams */
                vgmstream->num_samples = ps_bytes_to_samples(sb->stream_size, sb->channels) ;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            break;

        case RAW_XBOX:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case RAW_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = align_size_to_block(sb->stream_size / sb->channels, 0x04);

            /* DSP extra info entry size is 0x40 (first/last 0x10 = unknown), per channel */
            dsp_read_coefs_be(vgmstream,streamFile,sb->extra_offset + 0x10, 0x40);
            break;

        case FMT_VAG:
            /* skip VAG header (some sb4 use VAG and others raw PSX) */ //todo remove
            if (read_32bitBE(start_offset, streamData) == 0x56414770) { /* "VAGp" */
                start_offset += 0x30;
                sb->stream_size  -= 0x30;
            }

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb->stream_size / sb->channels;
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

            if (flag == 0x02 || flag == 0x04) {
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
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        case FMT_CWAV:
            if (sb->channels > 1) goto fail; //todo test
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;

            dsp_read_coefs_le(vgmstream,streamFile,start_offset + 0x7c, 0x40);
            start_offset += 0xe0; /* skip CWAV header */
            break;

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


static void build_readable_name(ubi_sb_header * sb, int bank_streams) {
    const char *grp_name;
    const char *res_name;
    uint32_t id;
    uint32_t type;
    int index;

    /* config */
    if (sb->is_map)
        grp_name = sb->map_name;
    else
        grp_name = "bank"; //NULL
    id = sb->header_id;
    type = sb->header_type;
    if (sb->is_map)
        index = sb->header_index; //bank_streams;
    else
        index = sb->header_index; //-1
    res_name = sb->resource_name;

    /* create name */
    if (grp_name) {
        if ((sb->is_external || sb->has_internal_names) && res_name[0]) {
            if (index >= 0)
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s/%04d/%02x-%08x/%s", grp_name, index, type, id, res_name);
            else
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s/%02x-%08x/%s", grp_name, type, id, res_name);
        }
        else {
            if (index >= 0)
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s/%04d/%02x-%08x", grp_name, index, type, id);
            else
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%s/%02x-%08x", grp_name, type, id);
        }
    }
    else {
        if ((sb->is_external || sb->has_internal_names) && res_name[0]) {
            if (index >= 0)
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%04d/%02x-%08x/%s", index, type, id, res_name);
            else
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%02x-%08x/%s", type, id, res_name);
        } else {
            if (index >= 0)
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%04d/%02x-%08x", index, type, id);
            else
                snprintf(sb->readable_name, sizeof(sb->readable_name), "%02x-%08x", type, id);
        }
    }
}

static int parse_header_type_audio(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sb->big_endian ? read_16bitBE : read_16bitLE;

    /* Single audio header, external or internal. Can be part of a sequence or separate
     * (some games don't use sequences at all). */

    sb->stream_size     = read_32bit(offset + sb->cfga_stream_size, streamFile);
    sb->extra_offset    = read_32bit(offset + sb->cfga_extra_offset, streamFile) + sb->sectionX_offset;
    sb->stream_offset   = read_32bit(offset + sb->cfga_stream_offset, streamFile);
    sb->channels        = (sb->has_short_channels) ?
                (uint16_t)read_16bit(offset + sb->cfga_channels, streamFile) :
                (uint32_t)read_32bit(offset + sb->cfga_channels, streamFile);
    sb->sample_rate     = read_32bit(offset + sb->cfga_sample_rate, streamFile);
    sb->stream_type     = read_32bit(offset + sb->cfga_stream_type, streamFile);

    if (sb->cfga_loop_flag) {
        sb->loop_flag = (read_32bit(offset + sb->cfga_loop_flag, streamFile) & sb->and_loop_flag);
    }

    if (sb->loop_flag) {
        sb->loop_start  = read_32bit(offset + sb->cfga_num_samples, streamFile);
        sb->num_samples = read_32bit(offset + sb->cfga_num_samples2, streamFile) + sb->loop_start;
        if (sb->has_full_loop) { /* early games just repeat and don't set loop start */
            sb->num_samples = sb->loop_start;
            sb->loop_start = 0;
        }
        /* loop starts that aren't 0 do exist but are very rare (ex. Beowulf PSP #33407)
         * also rare are looping external streams (ex. Surf's Up PSP #1462) */
    } else {
        sb->num_samples = read_32bit(offset + sb->cfga_num_samples, streamFile);
    }

    if (sb->cfga_group_id) {
        sb->group_id   = read_32bit(offset + sb->cfga_group_id, streamFile);
        if (sb->and_group_id) sb->group_id  &= sb->and_group_id;
        if (sb->shr_group_id) sb->group_id >>= sb->shr_group_id;
    }

    if (sb->cfga_external_flag) {
        sb->is_external = (read_32bit(offset + sb->cfga_external_flag, streamFile) & sb->and_external_flag);
    }

    /* external stream name can be found in the header (first versions) or the sectionX table (later versions) */
    if (sb->cfga_stream_name) {
        read_string(sb->resource_name, sb->resource_name_size, offset + sb->cfga_stream_name, streamFile);
    } else {
        sb->cfga_stream_name = read_32bit(offset + sb->cfga_extra_name, streamFile);
        if (sb->cfga_stream_name != 0xFFFFFFFF)
            read_string(sb->resource_name, sb->resource_name_size, sb->sectionX_offset + sb->cfga_stream_name, streamFile);
    }

    return 1;
//fail:
//    return 0;
}

static int parse_header_type_sequence(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
//    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    /* A "sequence" that includes N audio segments, defined as a chain of section2 entries.
     *
     * They don't include lead-in/outs and look loopable. Several sequences can reuse
     * audio segments (variations of the same songs), or can be single entries (pointing
     * to a full song or a lead-out).
     *
     * Sequences seem to include only music or dialogues, so even single entries may be useful to parse. */

    /* - rough format: */
    /* extra table offset (references id?) at ~0x0c */
    /* flags? */
    /* id? possibly related to sequence lead-out? */
    /* id? possibly related to sequence lead-in? */
    /* sequence count at ~0x28/2c */

    /* - in the extra table, per sequence count:
     * 0x00: section2 entry number (points to audio types)
     * 0x04+ size varies (0x10-0x14 are common)
     * at the end: some kind of ID?
     */

#if 0
    if (!sb->cfgs_sequence_count) {
        VGM_LOG("UBI SB: segment found but not configured at %lx\n", offset);
        goto fail;
    }

    return 1;
fail:
#endif
    return 0;
}

static int parse_header_type_layer(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
//    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
//    int16_t (*read_16bit)(off_t,STREAMFILE*) = sb->big_endian ? read_16bitBE : read_16bitLE;

//    goto fail;

    /* some values may be flags/config as multiple 0x06 can point to the same layer, with different 'flags'? */

    return 1;
//fail:
//    return 0;
}


static int parse_sb_header(ubi_sb_header * sb, STREAMFILE *streamFile, int target_subsong) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int i, j, k, bank_streams = 0, prev_streams;


    ;VGM_LOG("UBI SB: s1=%x (%x*%x), s2=%x (%x*%x), s3=%x (%x*%x), sX=%x (%x)\n",
            sb->section1_offset,sb->section1_entry_size,sb->section1_num,sb->section2_offset,sb->section2_entry_size,sb->section2_num,
            sb->section3_offset,sb->section3_entry_size,sb->section3_num,sb->sectionX_offset,sb->sectionX_size);

    prev_streams = sb->total_subsongs;

    /* find target stream info in section2 */
    for (i = 0; i < sb->section2_num; i++) {
        off_t offset = sb->section2_offset + sb->section2_entry_size*i;
        uint32_t header_id, header_type;

        /* parse base header (possibly called "resource" or "object") */
        header_id   = read_32bit(offset + 0x00, streamFile);
        header_type = read_32bit(offset + 0x04, streamFile);

        if (header_type <= 0x00 || header_type >= 0x10 || header_type == 0x09 || header_type == 0x0b) {
            VGM_LOG("UBI SB: unknown type %x at %x size %x\n", header_type, (uint32_t)offset, sb->section2_entry_size);
            goto fail;
        }

        //;VGM_ASSERT(header_type == 0x06 || header_type == 0x0d,
        //        "UBI SB: type %x at %x size %x\n", header_type, (uint32_t)offset, sb->section2_entry_size);

        sb->types[header_type]++;

        /* ignore non-audio entries */
        if (header_type != 0x01)
            continue;

        /* update streams (total_stream also doubles as current) */
        bank_streams++;
        sb->total_subsongs++;
        if (sb->total_subsongs != target_subsong)
            continue;

        /* parse target entry */
        sb->header_index    = i;
        sb->header_offset   = offset;

        sb->header_id       = header_id;
        sb->header_type     = header_type;

        switch(header_type) {
            case 0x01: /* old and new */
                if (!parse_header_type_audio(sb, offset, streamFile))
                    goto fail;
                break;
            case 0x02:
                /* A group, possibly to play with config. (ex: 0x08 (float 0.3) + 0x01) */
                goto fail;
            case 0x03:
          //case 0x09?
                /* A group, other way to play things? (ex: 0x03 + 0x04) */
                goto fail;
            case 0x04: /* newer/older */
            case 0x0a: /* older */
                /* A group of N audio/sequences, seemingly 'random' type to play one in the group
                 * (usually includes voice/sfx like death screams, but may include sequences).
                 * Header is similar to sequences (count in header, points to extra table's N entries in section2) */
                goto fail;
            case 0x05: /* newer */
            case 0x0c: /* older */
                if (!parse_header_type_sequence(sb, offset, streamFile))
                    goto fail;
                break;
            case 0x06: /* newer */
            case 0x0d: /* older */
                if (!parse_header_type_layer(sb, offset, streamFile))
                    goto fail;
                break;
            case 0x07:
          //case 0x0e?
                /* Another group of something (single entry?), rare. */
                goto fail;
            case 0x08: /* newer (also in older with 0x0f) */
            case 0x0f: /* older */
                /* Audio config? (almost all fields 0 except sometimes 1.0 float in the middle).
                 * In older games may also point to the extra table and look different, maybe equivalent to another type. */
                goto fail;

            default:
                /* debug strings reference:
                 * - TYPE_SAMPLE: should be 0x01 (also "sound resource")
                 * - TYPE_MULTITRACK: should be 0x06/0x0d (also "multilayer resource")
                 * - TYPE_SILENCE: ?
                 * sequences may be "theme resource"
                 *
                 * possible type names from .bnm (.sb's predecessor):
                 * 0: TYPE_INVALID
                 * 1: TYPE_SAMPLE
                 * 2: TYPE_MIDI
                 * 3: TYPE_CDAUDIO
                 * 4: TYPE_SEQUENCE
                 * 5: TYPE_SWITCH_OLD
                 * 6: TYPE_SPLIT
                 * 7: TYPE_THEME_OLD
                 * 8: TYPE_SWITCH
                 * 9: TYPE_THEME_OLD2
                 * A: TYPE_RANDOM
                 * B: TYPE_THEME0
                 */

                /* All types may contain memory garbage, making it harder to identify fields (platforms
                 * and games are affected differently by this). Often types contain memory from the previous
                 * type header unless overwritten, random memory, or default initialization garbage.
                 * So if some non-audio type looks like audio it's probably repeating old data.
                 * This even happens for common fields (ex. type 06 at 0x08 has prev garbage, not stream size). */
                goto fail;
        }


        /* maps can contain +10000 subsongs, we need something helpful */
        build_readable_name(sb, bank_streams);
    }

    ;VGM_LOG("UBI SB: types "); for (int i = 0; i < 16; i++) { VGM_ASSERT(sb->types[i], "%02x=%i ",i,sb->types[i]); } VGM_LOG("\n");


    if (sb->is_map) {
        if (bank_streams == 0 || target_subsong <= prev_streams || target_subsong > sb->total_subsongs)
            return 1; /* Target stream is not in this map */
    } else {
        if (sb->total_subsongs == 0) {
            VGM_LOG("UBI SB: no subsongs\n");
            goto fail;
        }

        if (target_subsong < 0 || target_subsong > sb->total_subsongs) {
            goto fail;
        }

        VGM_ASSERT(sb->section3_num > 2, "UBI SB: section3 > 2 found\n");
    }

    if (!(sb->cfga_group_id || sb->is_map) && sb->section3_num > 1) {
        VGM_LOG("UBI SB: unexpected number of internal stream groups %i\n", sb->section3_num);
        goto fail;
    }

    ;VGM_LOG("UBI SB: target at %x (cfg %x), extra=%x, name=%s, id=%i, t=%i\n",
            (uint32_t)sb->header_offset, sb->section2_entry_size, (uint32_t)sb->extra_offset, sb->resource_name, sb->group_id, sb->stream_type);


    /* happens in a few internal sounds from early Xbox games */
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
                case UBI_3DS:
                    sb->codec = FMT_CWAV;
                    break;

                default:
                    VGM_LOG("UBI SB: unknown internal format\n");
                    goto fail;
            }
            break;

        case 0x01: /* DSP (early games) or PCM (rarely used, ex. Wii/PSP/3DS) */
            switch (sb->version) {
                case 0x00000003: /* Donald Duck: Goin' Quackers */
                    sb->codec = RAW_DSP;
                    break;
                default:
                    sb->codec = RAW_PCM;
                    break;
            }
            break;

        case 0x02: /* PS ADPCM (PS3) */
            sb->codec = RAW_PSX;
            break;

        case 0x03: /* Ubi ADPCM (main external stream codec, has subtypes) */
            sb->codec = UBI_ADPCM;
            break;

        case 0x04: /* Ubi IMA v3 (early games) or Ogg (later PC games) */
            switch (sb->version) {
                case 0x00000007: /* Splinter Cell */
                    sb->codec = UBI_ADPCM;
                    break;
                default:
                    sb->codec = FMT_OGG;
                    break;
            }
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

        case 0x08: /* Ubi IMA v2/v3 (early games) or ATRAC3 */
            switch (sb->version) {
                case 0x00000003: /* Donald Duck: Goin' Quackers */
                case 0x00000004: /* Myst III: Exile */
                    sb->codec = UBI_ADPCM;
                    break;
                default:
                    sb->codec = FMT_AT3;
                    break;
            }
            break;

        default:
            VGM_LOG("UBI SB: unknown stream_type %x\n", sb->stream_type);
            goto fail;
    }

    if (sb->codec == RAW_XMA1) {
        /* this field is only seen in X360 games, points at XMA1 header in extra section */
        sb->xma_header_offset = read_32bit(sb->header_offset + sb->cfga_xma_offset, streamFile) + sb->sectionX_offset;
    }


    /* section 3: internal stream info */
    if (!sb->is_external) {
        /* Internal sounds are split into codec groups, with their offsets being relative to group start.
         * A table contains sizes of each group, so we adjust offsets based on the group ID of our sound.
         * Headers normally only use 0 or 1, and section3 may only define id1 (which the internal sound would use).
         * May exist even for external streams only, and they often use id 1 too. */

        if (sb->is_map) {
            /* maps store internal sounds offsets in a separate subtable, find the matching entry */
            for (i = 0; i < sb->section3_num; i++) {
                off_t offset = sb->section3_offset + 0x14 * i;
                off_t table_offset  = read_32bit(offset + 0x04, streamFile) + sb->section3_offset;
                uint32_t table_num  = read_32bit(offset + 0x08, streamFile);
                off_t table2_offset = read_32bit(offset + 0x0c, streamFile) + sb->section3_offset;
                uint32_t table2_num = read_32bit(offset + 0x10, streamFile);

                for (j = 0; j < table_num; j++) {
                    int index = read_32bit(table_offset + 0x08 * j + 0x00, streamFile) & 0x0000FFFF;

                    if (index == sb->header_index) {
                        if (!sb->cfga_group_id && table2_num > 1) {
                            VGM_LOG("UBI SB: unexpected number of internal stream map groups %i at %x\n", table2_num, (uint32_t)table2_offset);
                            goto fail;
                        }

                        sb->stream_offset = read_32bit(table_offset + 0x08 * j + 0x04, streamFile);
                        for (k = 0; k < table2_num; k++) {
                            /* entry layout:
                             * 0x00 - group ID
                             * 0x04 - size with padding included
                             * 0x08 - size without padding
                             * 0x0c - absolute offset */
                            uint32_t id = read_32bit(table2_offset + 0x10 * k + 0x00, streamFile);
                            if (id == sb->group_id) {
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

            /* banks store internal sounds offsets in table: group id + group size, find the matching entry */

            if (sb->cfga_group_id && sb->section3_num > 1) {
                for (i = 0; i < sb->section3_num; i++) {
                    off_t offset = sb->section3_offset + sb->section3_entry_size * i;

                    /* table has unordered ids+size, so if our id doesn't match current data offset must be beyond */
                    if (read_32bit(offset + 0x00, streamFile) == sb->group_id)
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

#if 0
static int config_sb_version_layer2(ubi_sb_header * sb) {

    sb->cfgc_sequence_count  = 0x28; //POP
    sb->cfg5_sequence_count  = 0x28; //POP WW
    sb->cfg5_sequence_count  = 0x2c; //POP TT, TMNT

    sb->cfgd_layer_rate     = 0x1c;
    sb->cfgd_layer_count    = 0x20;
    /* 0x2c: external flag? */
    sb->cfgd_stream_name    = 0x30;
    sb->cfgd_stream_offset  = 0x58;
    /* 0x5c: original layer rate? */
    sb->cfgd_stream_size    = 0x60;
    sb->cfgd_num_samples    = 0x64;
}
#endif
static int config_sb_version(ubi_sb_header * sb, STREAMFILE *streamFile) {
    int is_biadd_psp = 0;

    /* Type 1 audio header varies with almost every game + platform (some kind of struct serialization?)
     * Support is done case-by-case as offsets/order/fields change slightly. Header usually contains:
     * - fixed part (id, type, stream size, extra offset, stream offset)
     * - flags
     * - stream samples (early games may or may not set it for internal streams, even in the same game)
     * - stream size (same as the fixed part)
     * - bitrate / original sample rate
     * - sample rate
     * - pcm bits?
     * - channels
     * - stream type
     * - external filename or internal filename on some platforms (earlier versions)
     * - external filename offset in the extra table (later versions)
     * - end flags
     *
     * In between all those there are may be unused fields contain uninitialized memory garbage
     * (null, part of strings, data from previous header, etc).
     *
     * Earlier games or those with bigger header size have most fields, while later remove
     * or change around some, or sometimes only use fields for external streams.
     * We only configure offsets for fields actually needed.
     */


    /* common */
    sb->section3_entry_size = 0x08;
    sb->resource_name_size  = 0x24; /* maybe 0x28 or 0x20 for some but ok enough (null terminated) */
    /* this is same in all games since ~2003 */
    sb->cfga_stream_size    = 0x08;
    sb->cfga_extra_offset   = 0x0c;
    sb->cfga_stream_offset  = 0x10;
    sb->cfgs_extra_offset   = 0x0c;

    sb->and_external_flag   = 0x01;
    sb->and_loop_flag       = 0x01;


    /* Batman: Vengeance (2001)(PS2)-map 0x00000003 */
    /* Tom Clancy's Rainbow Six - Vegas 2 (2008)(PC)-? */
    /* Myst III (2008)(PS2)-? */


    //todo some dsp offsets have problems, wrong id?
    //todo uses Ubi IMA v2 has has some deviation in the right channel + clicks?
    //todo has some sample rate / loop configs problems? (ex Batman #5451)
    /* Disney's Tarzan: Untamed (2001)(GC)-map */
    /* Batman: Vengeance (2001)(GC)-map */
    /* Donald Duck: Goin' Quackers (2002)(GC)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x40;
        sb->section2_entry_size = 0x6c;

        sb->map_version = 1;

        sb->cfga_stream_size    = 0x0c;
        sb->cfga_extra_offset   = 0x10;
        sb->cfga_stream_offset  = 0x14;

        sb->cfga_group_id       = 0x2c;
        sb->cfga_external_flag  = 0x30;
        sb->cfga_loop_flag      = 0x34;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x48; /* full loop */
        sb->cfga_sample_rate    = 0x50;
        sb->cfga_channels       = 0x56;
        sb->cfga_stream_type    = 0x58;
        sb->cfga_stream_name    = 0x5c;

        sb->has_short_channels = 1;
        sb->has_full_loop = 1;

        sb->cfgs_extra_offset   = 0x10;
        sb->cfgs_sequence_count = 0x2c;
        //has layer 0d
        return 1;
    }
#if 0
    /* Batman: Vengeance (2001)(PS2)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x30;
        sb->section2_entry_size = 0x3c;

        sb->map_version = 1;

        sb->cfga_stream_size    = 0x0c;
        sb->cfga_extra_offset   = 0x10;
        sb->cfga_stream_offset  = 0x14;

        sb->cfga_group_id       = 0x1c?;
        sb->cfga_external_flag  = 0x1c?;
        sb->cfga_loop_flag      = 0x1c?;
        sb->cfga_num_samples    = 0x20? 28? 2c?
        sb->cfga_num_samples2   = 0x20? 28? 2c?
        sb->cfga_sample_rate    = 0x24;
        sb->cfga_channels       = 0x2a?
        sb->cfga_stream_type    = 0x34;
        sb->cfga_stream_name    = -1; //has implicit stream name

        sb->has_short_channels = 1;
        sb->has_full_loop = 1;

        //has layer 0d
        return 1;
    }
#endif

#if 0
    //todo offsets seems to work differently (stream offset is always 0)
    /* Myst III: Exile (2001)(PS2)-map */
    if (sb->version == 0x00000004 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x34;
        sb->section2_entry_size = 0x70;

        sb->map_version = 1;

        sb->cfga_stream_size    = 0x0c;
        sb->cfga_extra_offset   = 0x10;
        sb->cfga_stream_offset  = 0x14;

        sb->cfga_group_id       = 0x1c;
        sb->cfga_external_flag  = 0x1c;
        sb->cfga_loop_flag      = 0x1c;
        sb->cfga_channels       = 0x24;
        sb->cfga_sample_rate    = 0x28;
        sb->cfga_num_samples    = 0x2c;
        sb->cfga_num_samples2   = 0x34;
        sb->cfga_stream_name    = 0x44;
        sb->cfga_stream_type    = 0x6c;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;

        sb->cfgs_extra_offset   = 0x10;
        sb->cfgs_sequence_count = 0x2c;
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

        sb->cfga_stream_size    = 0x0c;
        sb->cfga_extra_offset   = 0x10;
        sb->cfga_stream_offset  = 0x14;

      //sb->cfga_loop_flag      = 0x24; //?
        sb->cfga_external_flag  = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_num_samples    = 0x30;
      //sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4a;
        sb->cfga_stream_type    = 0x4c;
        sb->cfga_stream_name    = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        //has layer 0d
        return 1;
    }
#endif
#if 0
    /* Splinter Cell (2002)(Xbox)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(Xbox)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_XBOX) {
        /* Stream types:
         * 0x01: PCM
         * 0x02: unsupported codec, appears to be Ubi IMA in a blocked layout
         * 0x04: Ubi IMA v3 (not Vorbis)
         */
        sb->section1_entry_size = 0x58;
        sb->section2_entry_size = 0x78;

        sb->map_version = 1;

        sb->cfga_stream_size    = 0x0c;
        sb->cfga_extra_offset   = 0x10;
        sb->cfga_stream_offset  = 0x14;

        sb->cfga_group_id       = 0x24? 0x2c;
        sb->cfga_external_flag  = 0x28;
        sb->cfga_loop_flag      = 0x2c? 0x24?
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4a;
        sb->cfga_stream_type    = 0x4c;
        sb->cfga_stream_name    = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;
        //has layer 0d
        return 1;
    }
#endif
    /* Prince of Persia: Sands of Time (2003)(PC)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PC) || /* (not sure if exists, just in case) */
        (sb->version == 0x000A0004 && sb->platform == UBI_PC)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x80;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4a;
        sb->cfga_stream_type    = 0x4c;
        sb->cfga_stream_name    = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;

        sb->cfgs_sequence_count  = 0x28;
        //has layer 0d (main game)
        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(PS2)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PS2) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_PS2)) { /* main game */
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x18;
        sb->cfga_loop_flag      = 0x18;
        sb->cfga_group_id       = 0x18;
        sb->cfga_channels       = 0x20;
        sb->cfga_sample_rate    = 0x24;
        sb->cfga_num_samples    = 0x30; /* may be null */
        sb->cfga_num_samples2   = 0x38; /* may be null */
        sb->cfga_stream_name    = 0x40;
        sb->cfga_stream_type    = 0x68;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;

        sb->cfgs_sequence_count = 0x28;
        //has layer 0d (main game)
        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(Xbox)-bank */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_XBOX) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_XBOX)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x78;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_group_id       = 0x28;
        sb->cfga_loop_flag      = 0x2c;
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4a;
        sb->cfga_stream_type    = 0x4c; /* may contain garbage */
        sb->cfga_stream_name    = 0x50;

        sb->has_short_channels = 1;
        sb->has_internal_names = 1;

        sb->cfgs_sequence_count  = 0x28;
        //has layer 0d (main game)
        return 1;
    }

    // todo fix batman interleave (ex. #22, #134, #222)
    /* Batman: Rise of Sin Tzu (2003)(GC)-map [0x000A0002] */
    /* Prince of Persia: Sands of Time (2003)(GC)-bank [0x000A0002/0x000A0004] */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_GC) || /* Prince of Persia 1 port */
        (sb->version == 0x000A0004 && sb->platform == UBI_GC)) { /* main game */
        sb->section1_entry_size = 0x64;
        sb->section2_entry_size = 0x74;

        sb->map_version = 2;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_group_id       = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_num_samples    = 0x2c;
        sb->cfga_num_samples2   = 0x34;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_channels       = 0x46;
        sb->cfga_stream_type    = 0x48;
        sb->cfga_stream_name    = 0x4c;

        sb->has_short_channels = 1;
        //has layer 0d (POP:SOT main game, Batman)

        sb->cfgs_sequence_count = 0x28;
        return 1;
    }

    /* Tom Clancy's Rainbow Six 3 (2003)(PS2)-bank */
    if (sb->version == 0x000A0007 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x18;
        sb->cfga_loop_flag      = 0x18;
        sb->cfga_group_id       = 0x18;
        sb->cfga_channels       = 0x20;
        sb->cfga_sample_rate    = 0x24;
        sb->cfga_num_samples    = 0x30; /* may be null */
        sb->cfga_num_samples2   = 0x38; /* may be null */
        sb->cfga_stream_name    = 0x40;
        sb->cfga_stream_type    = 0x68;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;

        //has layer 0d
        return 1;
    }

    /* Splincer Cell: Pandora Tomorrow(?) (2006)(PS2)-bank */
    if (sb->version == 0x000A0008 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x18;
        sb->cfga_loop_flag      = 0x18;
        sb->cfga_group_id       = 0x18;
        sb->cfga_channels       = 0x20;
        sb->cfga_sample_rate    = 0x24;
        sb->cfga_num_samples    = 0x30; /* may be null */
        sb->cfga_num_samples2   = 0x38; /* may be null */
        sb->cfga_stream_name    = 0x40;
        sb->cfga_stream_type    = 0x68;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;
        //has layer?
        return 1;
    }

    /* Myst IV Demo (2004)(PC)-bank */
    if (sb->version == 0x00100000 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0xa4;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4c;
        sb->cfga_stream_type    = 0x50;
        sb->cfga_stream_name    = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(PC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x84;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4c;
        sb->cfga_stream_type    = 0x50;
        sb->cfga_stream_name    = 0x54;

        sb->has_internal_names = 1;

        sb->cfgs_sequence_count  = 0x28;
        //no layers
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(PS2)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x18;
        sb->cfga_loop_flag      = 0x18;
        sb->cfga_group_id       = 0x18;
        sb->cfga_channels       = 0x20;
        sb->cfga_sample_rate    = 0x24;
        sb->cfga_num_samples    = 0x30; /* may be null */
        sb->cfga_num_samples2   = 0x38; /* may be null */
        sb->cfga_stream_name    = 0x40;
        sb->cfga_stream_type    = 0x68;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;

        sb->cfgs_sequence_count = 0x28;
        //no layers
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(Xbox)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x90;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_group_id       = 0x28;
        sb->cfga_loop_flag      = 0x40;
        sb->cfga_num_samples    = 0x44;
        sb->cfga_num_samples2   = 0x4c;
        sb->cfga_sample_rate    = 0x58;
        sb->cfga_channels       = 0x60;
        sb->cfga_stream_type    = 0x64; /* may contain garbage */
        sb->cfga_stream_name    = 0x68;

        sb->has_internal_names = 1;

        sb->cfgs_sequence_count  = 0x28;
        //no layers
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(GC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x6c;
        sb->section2_entry_size = 0x78;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_group_id       = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_num_samples    = 0x2c;
        sb->cfga_num_samples2   = 0x34;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_channels       = 0x48;
        sb->cfga_stream_type    = 0x4c;
        sb->cfga_stream_name    = 0x50;

        sb->cfgs_sequence_count = 0x28;
        //no layers
        return 1;
    }

    /* two configs with same id and both sb4/sm4; use project file as identifier */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "BIAAUDIO.SP4");
        if (streamTest) {
            is_biadd_psp = 1;
            close_streamfile(streamTest);
        }
    }

    /* Prince of Persia: Revelations (2005)(PSP)-bank */
    /* Splinter Cell: Essentials (2006)(PSP)-map */
    /* Beowulf: The Game (2007)(PSP)-map */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && !is_biadd_psp) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x84;

        sb->map_version = 2;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4c;
        sb->cfga_stream_type    = 0x50;
        sb->cfga_stream_name    = 0x54;

        sb->has_internal_names = 1;
        //has layers 06 (SC:E only)
        return 1;
    }

    //todo some .sb have bad external stream offsets (but not all, maybe unused garbage?)
    /* Brothers in Arms - D-Day (2006)(PSP)-bank */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && is_biadd_psp) {
        sb->section1_entry_size = 0x80;
        sb->section2_entry_size = 0x94;

        sb->cfga_external_flag  = 0x24;
        sb->cfga_loop_flag      = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4c;
        sb->cfga_stream_type    = 0x50;
        sb->cfga_stream_name    = 0x54;

        sb->has_internal_names = 1;
        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(PC)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x60;

        sb->map_version = 2;

        sb->cfga_external_flag  = 0x24;
      //sb->cfga_group_id       = 0x28;
      //sb->cfga_loop_flag      = 0x2c; //todo test
        sb->cfga_num_samples    = 0x30;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_sample_rate    = 0x44;
        sb->cfga_channels       = 0x4c;
        sb->cfga_stream_type    = 0x50;
        sb->cfga_extra_name     = 0x54;

        return 1;
    }

    /* Myst IV: Revelation (2005)(PC)-bank */
    /* Splinter Cell: Chaos Theory (2005)(Xbox)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x4c;

        sb->map_version = 2;

        sb->cfga_external_flag  = 0x18;
        sb->cfga_group_id       = 0x18;
        sb->cfga_loop_flag      = 0x18;
        sb->cfga_num_samples    = 0x1c;
        sb->cfga_num_samples2   = 0x24;
        sb->cfga_sample_rate    = 0x30;
        sb->cfga_channels       = 0x38;
        sb->cfga_stream_type    = 0x3c;
        sb->cfga_extra_name     = 0x40;

        sb->and_external_flag   = 0x0008;
        sb->and_loop_flag       = 0x0400;
        sb->and_group_id        = 0x0010;
        sb->shr_group_id        = 4;
        //no layers
        return 1;
    }

    /* Splinter Cell 3D (2011)(3DS)-map */
    if (sb->version == 0x00130001 && sb->platform == UBI_3DS) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x4c;

        sb->map_version = 2;

        sb->cfga_external_flag  = 0x18;
        sb->cfga_loop_flag      = 0x18;
        sb->cfga_group_id       = 0x18;
        sb->cfga_num_samples    = 0x1c;
        sb->cfga_num_samples2   = 0x24;
        sb->cfga_sample_rate    = 0x30;
        sb->cfga_channels       = 0x38;
        sb->cfga_stream_type    = 0x3c;
        sb->cfga_extra_name     = 0x40;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;
        //has layer 06
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x78;

        sb->cfga_external_flag  = 0x2c;
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_group_id       = 0x34;
        sb->cfga_num_samples    = 0x40;
        sb->cfga_num_samples2   = 0x48;
        sb->cfga_sample_rate    = 0x54;
        sb->cfga_channels       = 0x5c;
        sb->cfga_stream_type    = 0x60;
        sb->cfga_extra_name     = 0x64;

        sb->cfgs_sequence_count  = 0x2c;
        //no layers
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PS2)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x5c;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
        sb->cfga_group_id       = 0x20;
        sb->cfga_channels       = 0x2c;
        sb->cfga_sample_rate    = 0x30;
        sb->cfga_num_samples    = 0x3c;
        sb->cfga_num_samples2   = 0x44;
        sb->cfga_extra_name     = 0x4c;
        sb->cfga_stream_type    = 0x50;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;

        sb->cfgs_sequence_count  = 0x2c;
        //no layers
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(Xbox)-bank */
    /* Far Cry Instincts (2005)(Xbox)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
        sb->cfga_group_id       = 0x20;
        sb->cfga_num_samples    = 0x28;
        sb->cfga_num_samples2   = 0x30;
        sb->cfga_sample_rate    = 0x3c;
        sb->cfga_channels       = 0x44;
        sb->cfga_stream_type    = 0x48;
        sb->cfga_extra_name     = 0x4c;

        sb->and_external_flag   = 0x0008;
        sb->and_loop_flag       = 0x0400;
        sb->and_group_id        = 0x0010;
        sb->shr_group_id        = 4;

        sb->cfgs_sequence_count = 0x2c;
        //no layers
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(GC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_num_samples    = 0x3c;
        sb->cfga_num_samples2   = 0x44;
        sb->cfga_sample_rate    = 0x50;
        sb->cfga_channels       = 0x58;
        sb->cfga_stream_type    = 0x5c;
        sb->cfga_extra_name     = 0x60;

        sb->cfgs_sequence_count  = 0x2c;
        //no layers
        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(Xbox)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_group_id       = 0x20;
        sb->cfga_loop_flag      = 0x20;
        sb->cfga_num_samples    = 0x28;
        sb->cfga_num_samples2   = 0x30;
        sb->cfga_sample_rate    = 0x3c;
        sb->cfga_channels       = 0x44;
        sb->cfga_stream_type    = 0x48;
        sb->cfga_extra_name     = 0x4c;

        sb->and_external_flag   = 0x0008;
        sb->and_loop_flag       = 0x0400;
        sb->and_group_id        = 0x0010;
        sb->shr_group_id        = 4;
        //no layers
        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(GC)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_num_samples    = 0x3c;
        sb->cfga_num_samples2   = 0x44;
        sb->cfga_sample_rate    = 0x50;
        sb->cfga_channels       = 0x58;
        sb->cfga_stream_type    = 0x5c;
        sb->cfga_extra_name     = 0x60;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(PS2)-map */
    if (sb->version == 0x00160002 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
        sb->cfga_group_id       = 0x20;
        sb->cfga_channels       = 0x28;
        sb->cfga_sample_rate    = 0x2c;
        sb->cfga_num_samples    = 0x34;
        sb->cfga_num_samples2   = 0x38;
        sb->cfga_extra_name     = 0x44;
        sb->cfga_stream_type    = 0x48;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;

        return 1;
    }

    /* Far cry Instincts: Evolution (2006)(Xbox)-bank */
    if (sb->version == 0x00170000 && sb->platform == UBI_XBOX) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_group_id       = 0x20;
        sb->cfga_loop_flag      = 0x20;
        sb->cfga_num_samples    = 0x28;
        sb->cfga_num_samples2   = 0x30;
        sb->cfga_sample_rate    = 0x3c;
        sb->cfga_channels       = 0x44;
        sb->cfga_stream_type    = 0x48;
        sb->cfga_extra_name     = 0x4c;

        sb->and_external_flag   = 0x0008;
        sb->and_loop_flag       = 0x0400;
        sb->and_group_id        = 0x0010;
        sb->shr_group_id        = 4;
        return 1;
    }

    /* Open Season (2005)(PS2)-map [0x00180003] */
    /* Open Season (2005)(PSP)-map [0x00180003] */
    /* Prince of Persia: Rival Swords (2007)(PSP)-bank [0x00180005] */
    /* Rainbow Six Vegas (2007)(PSP)-bank [0x00180006] */
    /* Star Wars: Lethal Alliance (2006)(PSP)-map [0x00180007] */
    if ((sb->version == 0x00180003 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00180003 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180005 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180006 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180007 && sb->platform == UBI_PSP)) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x54;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
        sb->cfga_group_id       = 0x20;
        sb->cfga_channels       = 0x28;
        sb->cfga_sample_rate    = 0x2c;
        sb->cfga_num_samples    = 0x34;
        sb->cfga_num_samples2   = 0x3c;
        sb->cfga_extra_name     = 0x44;
        sb->cfga_stream_type    = 0x48;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        sb->and_group_id        = 0x08;
        sb->shr_group_id        = 3;
        //has layer 06
        return 1;
    }

    /* Red Steel (2006)(Wii)-bank */
    if (sb->version == 0x00180006 && sb->platform == UBI_WII) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_num_samples    = 0x3c;
        sb->cfga_num_samples2   = 0x44;
        sb->cfga_sample_rate    = 0x50;
        sb->cfga_channels       = 0x58;
        sb->cfga_stream_type    = 0x5c;
        sb->cfga_extra_name     = 0x60;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(PC)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x7c;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x2c;
      //sb->cfga_loop_flag      = 0x30; //todo test
        sb->cfga_group_id       = 0x34;
        sb->cfga_channels       = 0x5c;
        sb->cfga_sample_rate    = 0x54;
        sb->cfga_num_samples    = 0x40;
        sb->cfga_num_samples2   = 0x48;
        sb->cfga_stream_type    = 0x60;
        sb->cfga_extra_name     = 0x64;

        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(X360)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x78;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x2c;
        sb->cfga_group_id       = 0x30;
        sb->cfga_loop_flag      = 0x34;
        sb->cfga_channels       = 0x5c;
        sb->cfga_sample_rate    = 0x54;
        sb->cfga_num_samples    = 0x40;
        sb->cfga_num_samples2   = 0x48;
        sb->cfga_stream_type    = 0x60;
        sb->cfga_extra_name     = 0x64;
        sb->cfga_xma_offset     = 0x70;

        //has layer 06
        return 1;
    }

    /* TMNT (2007)(PSP)-map */
    if (sb->version == 0x00190001 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
      //sb->cfga_group_id       = 0x20; /* assumed */
        sb->cfga_channels       = 0x28;
        sb->cfga_sample_rate    = 0x2c;
        sb->cfga_num_samples    = 0x34;
        sb->cfga_num_samples2   = 0x3c;
        sb->cfga_stream_type    = 0x48;
        sb->cfga_extra_name     = 0x44;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        //has layer 06
        return 1;
    }

    /* TMNT (2007)(GC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_GC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x6c;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_stream_type    = 0x5c;

        return 1;
    }
    
    /* TMNT (2007)(PC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->cfga_external_flag  = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_stream_type    = 0x5c;

        return 1;
    }

    /* TMNT (2007)(PS2)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PS2) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x5c;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
      //sb->cfga_group_id       = 0x20; /* assumed */
        sb->cfga_channels       = 0x28;
        sb->cfga_sample_rate    = 0x2c;
        sb->cfga_num_samples    = 0x34;
        sb->cfga_num_samples2   = 0x3c;
        sb->cfga_extra_name     = 0x44;
        sb->cfga_stream_type    = 0x48;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        //has layer 06
        return 1;
    }

    /* TMNT (2007)(X360)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_stream_type    = 0x5c;
        sb->cfga_xma_offset     = 0x6c;

        return 1;
    }

    /* Prince of Persia: Rival Swords (2007)(Wii)-bank */
    if (sb->version == 0x00190003 && sb->platform == UBI_WII) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_stream_type    = 0x5c;

        //has layer 06 (TMNT)
        return 1;
    }

    /* Surf's Up (2007)(PC)-bank */
    if (sb->version == 0x00190005 && sb->platform == UBI_PC) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x74;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_stream_type    = 0x5c;

        return 1;
    }

    /* Surf's Up (2007)(PS3)-bank */
    /* Splinter Cell: Double Agent (2007)(PS3)-map */
    if (sb->version == 0x00190005 && sb->platform == UBI_PS3) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x28;
        sb->cfga_group_id       = 0x2c;
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_stream_type    = 0x5c;

        return 1;
    }

    /* Surf's Up (2007)(X360)-bank */
    if (sb->version == 0x00190005 && sb->platform == UBI_X360) {
        sb->section1_entry_size = 0x68;
        sb->section2_entry_size = 0x70;

        sb->cfga_external_flag  = 0x28;
      //sb->cfga_group_id       = 0x2c; /* assumed */
        sb->cfga_loop_flag      = 0x30;
        sb->cfga_channels       = 0x3c;
        sb->cfga_sample_rate    = 0x40;
        sb->cfga_num_samples    = 0x48;
        sb->cfga_num_samples2   = 0x50;
        sb->cfga_stream_type    = 0x5c;
        sb->cfga_extra_name     = 0x58;
        sb->cfga_xma_offset     = 0x6c;

        return 1;
    }

    /* Surf's Up (2007)(PSP)-map */
    if (sb->version == 0x00190005 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x48;
        sb->section2_entry_size = 0x58;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
      //sb->cfga_group_id       = 0x20; /* assumed */
        sb->cfga_channels       = 0x28;
        sb->cfga_sample_rate    = 0x2c;
        sb->cfga_num_samples    = 0x34;
        sb->cfga_num_samples2   = 0x3c;
        sb->cfga_extra_name     = 0x44;
        sb->cfga_stream_type    = 0x48;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x10;
        //no layers
        return 1;
    }

    /* Michael Jackson: The Experience (2010)(PSP)-map */
    if (sb->version == 0x001d0000 && sb->platform == UBI_PSP) {
        sb->section1_entry_size = 0x40;
        sb->section2_entry_size = 0x60;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x20;
        sb->cfga_loop_flag      = 0x20;
      //sb->cfga_group_id       = 0x20; /* assumed */
        sb->cfga_channels       = 0x28;
        sb->cfga_sample_rate    = 0x30;
        sb->cfga_num_samples    = 0x38;
        sb->cfga_num_samples2   = 0x40;
        sb->cfga_extra_name     = 0x48;
        sb->cfga_stream_type    = 0x4c;

        sb->and_external_flag   = 0x04;
        sb->and_loop_flag       = 0x20;
        //no layers
        return 1;
    }

    /* Splinter Cell Classic Trilogy HD (2011)(PS3)-map */
    if (sb->version == 0x001d0000 && sb->platform == UBI_PS3) {
        sb->section1_entry_size = 0x5c;
        sb->section2_entry_size = 0x80;

        sb->map_version = 3;

        sb->cfga_external_flag  = 0x28;
        sb->cfga_group_id       = 0x30;
        sb->cfga_loop_flag      = 0x34;
        sb->cfga_channels       = 0x44;
        sb->cfga_sample_rate    = 0x4c;
        sb->cfga_num_samples    = 0x54;
        sb->cfga_num_samples2   = 0x5c;
        sb->cfga_extra_name     = 0x64;
        sb->cfga_stream_type    = 0x68;

        //has layer 06
        return 1;
    }

    VGM_LOG("UBI SB: unknown SB/SM version+platform for %08x\n", sb->version);
    return 0;
}

/* Donald Duck: Goin' Quackers (2002)(GC)-map */
/* - type header:
 * 0x1c: sample rate * layers
 * 0x20: layers
 * 0x30: external flag?
 * 0x34: external name
 * 0x44: stream offset
 * 0x48: original rate * layers?
 * 0x4c: stream size (not including padding)
 * no samples
 *
 * - layer header
 * - blocked data
 * 0x02 see below (Ubi IMA v2 though)
 */

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
 * 0x00: version? (0x00000002)
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


/* Rainbow Six 3 */
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
 * 0x00: version? (0x00000004)
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
 * 0x5c: sample rate * layers
 * 0x60: stream size
 * 0x64: stream offset?
 *
 * - in header extra offset (style 0x10)
 * 0x00: sample rate
 * 0x04: 16?
 * 0x08: channels?
 * 0x0c: codec?
 *
 * - layer header at stream_offset:
 * 0x00: version (0x00000007)
 * 0x04: config?
 * 0x08: layers
 * 0x0c: stream size
 * 0x10: blocks count
 * 0x14: block header size
 * 0x18: next block size
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

/* Splinter Cell: Double Agent (2006)(X360)-map 0x00180006 */
/* - type header:
 * 0x08: header extra offset
 * 0x20: layers
 * 0x28: external flag?
 * 0x30: external flag?
 * 0x34: sample rate * layers
 * 0x38: stream size
 * 0x3c: stream offset
 * 0x44: name extra offset
 *
 * - in header extra offset
 * style 0x10 (codec 05 XMA), possible total size 0x34
 *
 * - layer header at stream_offset:
 * version 0x000C0008 same as 0x000B0008, but codec header size is 0
 *
 * - blocked data:
 * version same as 0x000B0008 (including mini XMA header + weird tables)
 */

/* Splinter Cell 3D (2011)(3DS)-map 0x00130001 */
/* - type header:
 * 0x08: header extra offset
 * 0x10: layers
 * 0x28: stream size
 * 0x30: stream offset
 * 0x34: name extra offset
 *
 * - in header extra offset
 * style 0x10 (size 0x18?)
 *
 * - layer header at stream_offset:
 * same as 0x00000007 but weird header thing is 0x00~0x08, has header sizes after that
 *
 * - blocked data:
 * same as 0x00000007
 */

/* TMNT (2007)(PS2)-bank 0x00190002 */
/* - type header:
 * 0x08: header extra offset
 * 0x1c: external flag?
 * 0x20: layers
 * 0x28: sample rate * layers
 * 0x2c: stream size
 * 0x30: stream offset
 * 0x38: name extra offset
 *
 * - in header extra offset (style 0x0c)
 * 0x00: sample rate
 * 0x04: channels
 * 0x08: codec
 *
 * - layer header at stream_offset:
 * 0x00: version? (0x000B0008)
 * 0x04: config? (0x00/0x0e/0x0b/etc)
 * 0x08: layers
 * 0x0c: blocks count
 * 0x10: block header size
 * 0x14: size of header sizes
 * 0x18: next block size
 * - per layer:
 * 0x00: layer header size
 * - per layer (if layer header size > 0)
 * 0x00~0x20: standard Ubi IMA header (version 0x05), PCM data
 *
 * - blocked data:
 * 0x00: always 0x03
 * 0x04: next block size
 * - per layer:
 * 0x00: layer data size (varies between blocks, and one layer may have more than other)
 */

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
 * style 0x10 (codec 03=Ubi, 01=PCM16LE in SW:LA/RS:V)
 * - layer header at stream_offset:
 * - blocked data:
 * version 0x000B0008
 */

/* TMNT (2007)(PSP)-map 0x00190001 */
/* - type header:
 * 0x0c: header extra offset
 * 0x20: layers
 * 0x24: total channels?
 * 0x28: sample rate * layers?
 * 0x2c: stream size
 * 0x30: stream offset
 * 0x38: name extra offset
 *
 * - in header extra offset
 * style 0x0c
 * - layer header at stream_offset:
 * - blocked data:
 * version 0x000B0008, but codec header size is 0
 */

/* TMNT (2007)(GC)-bank 0x00190002 */
/* Surf's Up (PS3)-bank 0x00190005 */
/* - type header:
 * 0x08: header extra offset
 * 0x1c: external flag?
 * 0x20: layers
 * 0x2c: external flag? (would match header 01)
 * 0x30: sample rate * layers
 * 0x34: stream size
 * 0x38: stream offset
 * 0x40: name extra offset
 *
 * - in header extra offset
 * style 0x0c
 * - layer header at stream_offset:
 * - blocked data:
 * version 0x000B0008
 */

/* Splinter Cell Classic Trilogy HD (2011)(PS3)-map 0x001d0000 */
/* - type header:
 * 0x0c: header extra offset
 * 0x20: layers
 * 0x44: stream size
 * 0x48: stream offset
 * 0x54: name extra offset
 *
 * - in header extra offset
 * style 0x0c
 * - layer header at stream_offset:
 * - blocked data:
 * version 0x000B0008, but codec header size is 0
 */
