#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

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
} ivaud_header;

static int parse_ivaud_header(STREAMFILE* sf, ivaud_header* ivaud);


/* .ivaud - from GTA IV (PC/PS3/X360) */
VGMSTREAM* init_vgmstream_ivaud(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    ivaud_header ivaud = {0};
    int loop_flag;

    /* checks */
    /* (hashed filenames are likely extensionless and .ivaud is added by tools) */
    if (!check_extensions(sf, "ivaud,"))
        goto fail;

    /* check header */
    if (!parse_ivaud_header(sf, &ivaud))
        goto fail;


    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ivaud.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ivaud.sample_rate;
    vgmstream->num_samples = ivaud.num_samples;
    vgmstream->num_streams = ivaud.total_subsongs;
    vgmstream->stream_size = ivaud.stream_size;
    vgmstream->meta_type = meta_IVAUD;

    switch(ivaud.codec) {
        case 0x0001: /* common in sfx, uncommon in music (ex. EP2_SFX/MENU_MUSIC) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = ivaud.is_music ? layout_blocked_ivaud : layout_none;
            vgmstream->full_block_size = ivaud.block_size;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0000: { /* XMA2 (X360) */
            uint8_t buf[0x100];
            size_t bytes;

            if (ivaud.is_music) {
                goto fail;
            }
            else {
                /* regular XMA for sfx */
                bytes = ffmpeg_make_riff_xma1(buf, 0x100, ivaud.num_samples, ivaud.stream_size, ivaud.channel_count, ivaud.sample_rate, 0);
                vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, ivaud.stream_offset, ivaud.stream_size);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;

                xma_fix_raw_samples(vgmstream, sf, ivaud.stream_offset, ivaud.stream_size, 0, 0,0); /* samples are ok? */
            }
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0100: { /* MPEG (PS3) */
            mpeg_custom_config cfg = {0};

            if (ivaud.is_music) {
                goto fail;
            }
            else {
                cfg.chunk_size = ivaud.block_size;
                cfg.big_endian = ivaud.big_endian;

                vgmstream->codec_data = init_mpeg_custom(sf, ivaud.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
            }
            break;
        }
#endif

        case 0x0400: /* PC */
            vgmstream->coding_type = coding_IMA_int;
            vgmstream->layout_type = ivaud.is_music ? layout_blocked_ivaud : layout_none;
            vgmstream->full_block_size = ivaud.block_size;
            break;

        default:
            VGM_LOG("IVAUD: unknown codec 0x%x\n", ivaud.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,sf,ivaud.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Parse Rockstar's .ivaud header (much info from SparkIV). */
static int parse_ivaud_header(STREAMFILE* sf, ivaud_header* ivaud) {
    int target_subsong = sf->stream_index;
    uint64_t (*read_u64)(off_t,STREAMFILE*);
    uint32_t (*read_u32)(off_t,STREAMFILE*);
    uint16_t (*read_u16)(off_t,STREAMFILE*);


    ivaud->big_endian = read_u32be(0x00, sf) == 0; /* table offset at 0x04 > BE (64b) */
    read_u64 = ivaud->big_endian ? read_u64be : read_u64le;
    read_u32 = ivaud->big_endian ? read_u32be : read_u32le;
    read_u16 = ivaud->big_endian ? read_u16be : read_u16le;

    /* use bank's stream count to detect */
    ivaud->is_music = (read_u32(0x10,sf) == 0);

    if (ivaud->is_music)  {
        off_t block_table_offset, channel_table_offset, channel_info_offset;

        /* music header */
        block_table_offset = read_u64(0x00,sf);
        ivaud->block_count = read_u32(0x08,sf);
        ivaud->block_size = read_u32(0x0c,sf); /* uses padded blocks */
        /* 0x10(4): stream count  */
        channel_table_offset = read_u64(0x14,sf);
        /* 0x1c(8): block_table_offset again? */
        ivaud->channel_count = read_u32(0x24,sf);
        /* 0x28(4): unknown entries? */
        ivaud->stream_offset = read_u32(0x2c,sf);
        channel_info_offset = channel_table_offset + ivaud->channel_count * 0x10;

        if ((ivaud->block_count * ivaud->block_size) + ivaud->stream_offset != get_streamfile_size(sf)) {
            VGM_LOG("IVAUD: bad file size\n");
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
        ivaud->num_samples = read_u32(channel_info_offset+0x10,sf);
        /* 0x14(4): unknown (-1) */
        /* 0x18(2): sample rate */
        /* 0x1a(2): unknown */
        ivaud->codec = read_u32(channel_info_offset+0x1c,sf);
        /* (when codec is IMA) */
        /* 0x20(8): adpcm states offset, 0x38: num states? (reference for seeks?) */
        /* rest: unknown data */

        /* block table (one entry per block) */
        /* 0x00: data size processed up to this block (doesn't count block padding) */
        ivaud->sample_rate = read_u32(block_table_offset + 0x04,sf);
        /* sample_rate should agree with each channel in the channel table */


        ivaud->total_subsongs = 1;
        ivaud->stream_size = get_streamfile_size(sf);
    }
    else {
        off_t stream_table_offset, stream_info_offset, stream_entry_offset, offset;

        /* bank header */
        stream_table_offset = read_u64(0x00,sf);
        /* 0x08(8): header size? start offset? */
        ivaud->total_subsongs = read_u32(0x10,sf);
        /* 0x14(4): unknown */
        ivaud->stream_offset = read_u32(0x18,sf); /* base start_offset */

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > ivaud->total_subsongs || ivaud->total_subsongs < 1) goto fail;

        if (stream_table_offset != 0x1c)
            goto fail;
        stream_info_offset = stream_table_offset + 0x10*ivaud->total_subsongs;

        /* stream table (one entry per stream, points to stream info) */
        stream_entry_offset = read_u64(stream_table_offset + 0x10*(target_subsong-1) + 0x00,sf); /* within stream info */
        /* 0x00(8): offset within stream_info_offset */
        /* 0x08(4): hash */
        /* 0x0c(4): some offset/size */

        /* stream info (one entry per stream) */
        offset = stream_info_offset + stream_entry_offset;
        ivaud->stream_offset += read_u64(offset+0x00,sf); /* within data */
        /* 0x08(4): hash */
        ivaud->stream_size = read_u32(offset+0x0c,sf);
        ivaud->num_samples = read_u32(offset+0x10,sf);
        /* 0x14(4): unknown (-1) */
        ivaud->sample_rate = read_u16(offset+0x18,sf);
        /* 0x1a(2): unknown */
        ivaud->codec = read_u32(offset+0x1c,sf);
        /* (when codec is IMA) */
        /* 0x20(8): adpcm states offset, 0x38: num states? (reference for seeks?) */
        /* rest: unknown data */

        ivaud->channel_count = 1;
    }


    return 1;
fail:
    return 0;
}
