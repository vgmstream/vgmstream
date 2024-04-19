#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

typedef struct {
    int is_music;

    int total_subsongs;

    int channel_count;
    int sample_rate;
    int codec;
    int num_samples;

    size_t block_count;
    size_t block_size;

    off_t stream_offset;
    size_t stream_size;

    int big_endian;
} aud_header;

static int parse_aud_header(STREAMFILE* sf, aud_header* aud);


/* RAGE AUD - MC:LA, GTA IV (PC/PS3/X360) */
VGMSTREAM* init_vgmstream_rage_aud(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    aud_header aud = {0};
    int loop_flag;

    /* checks */
    /* extensionless (.ivaud is fake/added by tools) */
    if (!check_extensions(sf, "ivaud,"))
        return NULL;

    /* check header */
    if (!parse_aud_header(sf, &aud))
        return NULL;


    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(aud.channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = aud.sample_rate;
    vgmstream->num_samples = aud.num_samples;
    vgmstream->num_streams = aud.total_subsongs;
    vgmstream->stream_size = aud.stream_size;
    vgmstream->codec_endian = aud.big_endian;
    vgmstream->meta_type = meta_RAGE_AUD;

    switch (aud.codec) {
        case 0x0001: /* common in sfx, uncommon in music (ex. EP2_SFX/MENU_MUSIC) */
            vgmstream->coding_type = aud.big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = aud.is_music ? layout_blocked_rage_aud : layout_none;
            vgmstream->full_block_size = aud.block_size;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0000: { /* XMA2 (X360) */
            if (aud.is_music) {
                goto fail;
            }
            else {
                /* regular XMA for sfx */
                vgmstream->codec_data = init_ffmpeg_xma1_raw(sf, aud.stream_offset, aud.stream_size, aud.channel_count, aud.sample_rate, 0);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;

                xma_fix_raw_samples(vgmstream, sf, aud.stream_offset, aud.stream_size, 0, 0, 0); /* samples are ok? */
            }
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0100: { /* MPEG (PS3) */
            mpeg_custom_config cfg = {0};

            if (aud.is_music) {
                goto fail;
            }
            else {
                cfg.chunk_size = aud.block_size;
                cfg.big_endian = aud.big_endian;

                vgmstream->codec_data = init_mpeg_custom(sf, aud.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
            }
            break;
        }
#endif

        case 0x0400: /* PC */
            vgmstream->coding_type = coding_IMA_int;
            vgmstream->layout_type = aud.is_music ? layout_blocked_rage_aud : layout_none;
            vgmstream->full_block_size = aud.block_size;
            break;

        default:
            VGM_LOG("RAGE AUD: unknown codec 0x%x\n", aud.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, aud.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Parse Rockstar's AUD header (much info from SparkIV). */
static int parse_aud_header(STREAMFILE* sf, aud_header* aud) {
    int target_subsong = sf->stream_index;
    read_u64_t read_u64;
    read_u32_t read_u32;
    read_u16_t read_u16;


    aud->big_endian = read_u32be(0x00, sf) == 0; /* table offset at 0x04 > BE (64b) */
    read_u64 = aud->big_endian ? read_u64be : read_u64le;
    read_u32 = aud->big_endian ? read_u32be : read_u32le;
    read_u16 = aud->big_endian ? read_u16be : read_u16le;

    uint64_t table_offset = read_u64(0x00, sf);
    if (table_offset > 0x10000) /* arbitrary max, typically 0x1c~0x1000 */
        return 0;

    /* use bank's stream count to detect */
    aud->is_music = (read_u32(0x10, sf) == 0);

    if (aud->is_music) {
        off_t block_table_offset, channel_table_offset, channel_info_offset;

        /* music header */
        block_table_offset = table_offset;
        aud->block_count = read_u32(0x08, sf);
        aud->block_size = read_u32(0x0c, sf); /* uses padded blocks */
        /* 0x10(4): stream count  */
        channel_table_offset = read_u64(0x14, sf);
        /* 0x1c(8): block_table_offset again? */
        aud->channel_count = read_u32(0x24, sf);
        /* 0x28(4): unknown entries? */
        aud->stream_offset = read_u32(0x2c, sf);
        channel_info_offset = channel_table_offset + aud->channel_count * 0x10;

        if ((aud->block_count * aud->block_size) + aud->stream_offset != get_streamfile_size(sf)) {
            VGM_LOG("RAGE AUD: bad file size\n");
            goto fail;
        }

        /* channel table (one entry per channel, points to channel info) */
        /* 0x00(8): offset within channel_info_offset */
        /* 0x08(4): hash */
        /* 0x0c(4): size */

        /* channel info (one entry per channel) */
        /* 0x00(8): offset within data (should be 0) */
        /* 0x08(4): hash */
        /* 0x0c(4): half num_samples? */
        aud->num_samples = read_u32(channel_info_offset + 0x10, sf);
        /* 0x14(4): unknown (-1) */
        /* 0x18(2): sample rate */
        /* 0x1a(2): unknown */
        aud->codec = read_u32(channel_info_offset + 0x1c, sf);
        /* (when codec is IMA) */
        /* 0x20(8): adpcm states offset, 0x38: num states? (reference for seeks?) */
        /* rest: unknown data */

        /* block table (one entry per block) */
        /* 0x00: data size processed up to this block (doesn't count block padding) */
        aud->sample_rate = read_u32(block_table_offset + 0x04, sf);
        /* sample_rate should agree with each channel in the channel table */


        aud->total_subsongs = 1;
        aud->stream_size = get_streamfile_size(sf);
    }
    else {
        off_t stream_table_offset, stream_info_offset, stream_entry_offset, offset;

        /* bank header */
        stream_table_offset = table_offset;
        /* 0x08(8): header size? start offset? */
        aud->total_subsongs = read_u32(0x10, sf);
        /* 0x14(4): unknown */
        aud->stream_offset = read_u32(0x18, sf); /* base start_offset */

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > aud->total_subsongs || aud->total_subsongs < 1) goto fail;

        if (stream_table_offset != 0x1c)
            goto fail;
        stream_info_offset = stream_table_offset + 0x10 * aud->total_subsongs;

        /* stream table (one entry per stream, points to stream info) */
        stream_entry_offset = read_u64(stream_table_offset + 0x10 * (target_subsong - 1) + 0x00, sf); /* within stream info */
        /* 0x00(8): offset within stream_info_offset */
        /* 0x08(4): hash */
        /* 0x0c(4): some offset/size */

        /* stream info (one entry per stream) */
        offset = stream_info_offset + stream_entry_offset;
        aud->stream_offset += read_u64(offset + 0x00, sf); /* within data */
        /* 0x08(4): hash */
        aud->stream_size = read_u32(offset + 0x0c, sf);
        aud->num_samples = read_u32(offset + 0x10, sf);
        /* 0x14(4): unknown (-1) */
        aud->sample_rate = read_u16(offset + 0x18, sf);
        /* 0x1a(2): unknown */
        aud->codec = read_u32(offset + 0x1c, sf);
        /* (when codec is IMA) */
        /* 0x20(8): adpcm states offset, 0x38: num states? (reference for seeks?) */
        /* rest: unknown data */

        aud->channel_count = 1;
    }

    return 1;

fail:
    return 0;
}
