#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"
#include "../util/endianness.h"
#include "rage_aud_streamfile.h"

typedef struct {
    int big_endian;
    int is_streamed; /* implicit: streams=music, sfx=memory */

    int total_subsongs;

    int channels;
    int sample_rate;
    int num_samples;
    int codec;

    int block_count;
    int block_chunk;

    uint32_t stream_offset;
    uint32_t stream_size;
} aud_header;

static bool parse_aud_header(STREAMFILE* sf, aud_header* aud);

static layered_layout_data* build_layered_rage_aud(STREAMFILE* sf, aud_header* aud);


/* RAGE AUD - from older RAGE engine games [Midnight Club: Los Angeles (PS3/X360), GTA IV (PC/PS3/X360)] */
VGMSTREAM* init_vgmstream_rage_aud(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    aud_header aud = {0};
    int loop_flag;

    /* checks */
    /* (extensionless): original names before hashing
     * .ivaud: fake/added by tools */
    if (!check_extensions(sf, ",ivaud"))
        return NULL;

    /* check header */
    if (!parse_aud_header(sf, &aud))
        return NULL;


    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(aud.channels, loop_flag);
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
            vgmstream->layout_type = aud.is_streamed ? layout_blocked_rage_aud : layout_none;
            vgmstream->full_block_size = aud.block_chunk;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0000: { /* XMA1 (X360) */
            if (aud.is_streamed) {
                vgmstream->layout_data = build_layered_rage_aud(sf, &aud);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_FFmpeg;
            }
            else {
                /* regular XMA for sfx */
                vgmstream->codec_data = init_ffmpeg_xma1_raw(sf, aud.stream_offset, aud.stream_size, aud.channels, aud.sample_rate, 0);
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
            if (aud.is_streamed) {
                vgmstream->layout_data = build_layered_rage_aud(sf, &aud);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_MPEG_custom;
            }
            else {
                vgmstream->codec_data = init_mpeg_custom(sf, aud.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, NULL);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
            }

            /* RAGE MP3s have an odd encoder delay handling: files are encoded ignoring the first 1152
             * samples, then MP3s play 1 frame of silence (encoder delay) and rest of the song. This
             * makes waveforms correctly aligned vs other platforms without having to manually discard
             * samples, but don't actually contain the first samples of a song. */
            break;
        }
#endif

        case 0x0400: /* PC */
            vgmstream->coding_type = coding_IMA_mono;
            vgmstream->layout_type = aud.is_streamed ? layout_blocked_rage_aud : layout_none;
            vgmstream->full_block_size = aud.block_chunk;
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
static bool parse_aud_header(STREAMFILE* sf, aud_header* aud) {
    int target_subsong = sf->stream_index;
    read_u64_t read_u64;
    read_u32_t read_u32;
    read_u16_t read_u16;


    aud->big_endian = read_u32be(0x00, sf) == 0; /* table offset at 0x04 > BE (64b) */
    read_u64 = aud->big_endian ? read_u64be : read_u64le;
    read_u32 = aud->big_endian ? read_u32be : read_u32le;
    read_u16 = aud->big_endian ? read_u16be : read_u16le;

    uint64_t table_offset = read_u64(0x00, sf);
    if (table_offset > 0x20000 || table_offset < 0x1c) /* typically 0x1c~0x1000, seen ~0x19000 */
        return false;

    /* use bank's stream count to detect */
    aud->is_streamed = (read_u32(0x10, sf) == 0);

    if (aud->is_streamed) {
        off_t block_table_offset, channel_table_offset, channel_info_offset;

        /* music header */
        block_table_offset = table_offset;
        aud->block_count = read_u32(0x08, sf);
        aud->block_chunk = read_u32(0x0c, sf); /* uses padded blocks */
        /* 0x10(4): stream count  */
        channel_table_offset = read_u64(0x14, sf);
        /* 0x1c(8): block_table_offset again? */
        aud->channels = read_u32(0x24, sf);
        /* 0x28(4): unknown entries? */
        aud->stream_offset = read_u32(0x2c, sf);
        channel_info_offset = channel_table_offset + aud->channels * 0x10;

        /* block count is off in rare XMA streams, though we only need it to check the header:
         *  GTA4 - Header says 2 blocks, actually has 3 - EP1_SFX/RP03_ML
         *  MCLA - Header says 3 blocks, actually has 4 - AUDIO/X360/SFX/AMBIENCE_STREAM/AMB_QUADSHOT_MALL_ADVERT_09
         */
        uint32_t expected_size = aud->block_count * aud->block_chunk + aud->stream_offset;
        if (expected_size != get_streamfile_size(sf) && expected_size + aud->block_chunk != get_streamfile_size(sf)) {
            //;VGM_LOG("RAGE AUD: bad file size\n");
            return false;
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
        /* rest: unknown data (varies per codec?) */
        /* in MC:LA there is samples-per-frame and frame size table for MPEG */

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

        if (stream_table_offset != 0x1c)
            return false;
        if (!check_subsongs(&target_subsong, aud->total_subsongs))
            return false;

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

        /* GTA IV PS3's 0x7D27B1E8' #165 and #166 seem to point to the same wrong offset pointing to the middle of data
         * (original bug/unused? maybe should allow MPEG resync?) */

        aud->channels = 1;
    }

    return true;
}

/* ************************************************************************* */

static VGMSTREAM* build_blocks_vgmstream(STREAMFILE* sf, aud_header* aud, int channel) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int block_channels = 1;
    uint32_t substream_size, substream_offset;


    /* setup custom IO streamfile that removes RAGE's odd blocks (not perfect but serviceable) */
    temp_sf = setup_rage_aud_streamfile(sf, aud->stream_offset, aud->stream_size, aud->block_chunk, aud->channels, channel, aud->codec, aud->big_endian);
    if (!temp_sf) goto fail;

    substream_offset = 0x00;
    substream_size = get_streamfile_size(temp_sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(block_channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RAGE_AUD;
    vgmstream->sample_rate = aud->sample_rate;
    vgmstream->num_samples = aud->num_samples;
    vgmstream->stream_size = aud->stream_size;

    vgmstream->stream_size = substream_size;


    switch(aud->codec) {
#ifdef VGM_USE_FFMPEG
        case 0x0000: {    /* XMA1 (X360) */
            vgmstream->codec_data = init_ffmpeg_xma1_raw(temp_sf, substream_offset, substream_size, block_channels, aud->sample_rate, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            //xma_fix_raw_samples(vgmstream, temp_sf, substream_offset, substream_size, 0, 0,0); /* samples are ok? */
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0100: { /* MPEG (PS3) */
            vgmstream->codec_data = init_mpeg_custom(temp_sf, substream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, temp_sf, substream_offset))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;
fail:
    ;VGM_LOG("RAGE AUD: can't open decoder\n");
    close_vgmstream(vgmstream);
    close_streamfile(temp_sf);
    return NULL;
}

/* ************************************************************************* */

/* blah blah, see awc.c */
static layered_layout_data* build_layered_rage_aud(STREAMFILE* sf, aud_header* aud) {
    int i;
    layered_layout_data* data = NULL;


    /* init layout */
    data = init_layout_layered(aud->channels);
    if (!data) goto fail;

    /* open each layer subfile */
    for (i = 0; i < aud->channels; i++) {
        data->layers[i] = build_blocks_vgmstream(sf, aud, i);
        if (!data->layers[i]) goto fail;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    return data;
fail:
    free_layout_layered(data);
    return NULL;
}
