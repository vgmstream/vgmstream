#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

/*
 * CF_DF - CyberFlix DreamFactory Engine
 *
 * Structure:
 * - 0x400-byte header: file size @0x04, container count @0x14; later files use
 *   "LPPALPPA" @0x20.
 * - u32 offset table @0x400 pointing to each container (u32 id, u32 size, then payload).
 * - Later audio chunks hold: u16 codec @0x1A (1=v4.0 / 2=v4.1), u32 rate @0x1C,
 *   u32 uncompressed size @0x24, u32 data offset @0x2C.
 * - Proto audio resources hold a native-endian u16 duration followed by v4.0 ADPCM.
 *
 * Container 1 holds a "loop block" describing a playback track: a chunkOrder sequence plus a
 * list of audio chunks. A second "single block" (pointed at from container 0) lists named
 * one-shot chunks. subsong 1 = the assembled track, subsongs 2..N = every audio
 * chunk individually.
 *
 * Titles: Lunicus, Jump Raven, Dust 1996 3.1/95, Titanic: Adventure Out of Time
 * Disney's Math/Reading Quest with Aladdin
 *
 */

#define DF_HEADER_SIZE  0x400
#define DF_ORDER_REGION 0x104   /* fixed-size chunkOrder area in the loop block */
#define DF_NAME_OFFSET  0x0A    /* Pascal length byte within named entries */
#define DF_LOOP_ENTRY   0x1A    /* loop/non-MOV entry stride: 16-byte Pascal field */
#define DF_MOV_ENTRY    0x2A    /* MOV one-shot entry stride: 32-byte Pascal field */
#define DF_SHORT_NAME_MAX (DF_LOOP_ENTRY - DF_NAME_OFFSET - 1)
#define DF_MOV_NAME_MAX   (DF_MOV_ENTRY - DF_NAME_OFFSET - 1)
#define DF_MAX_CHUNKS   0x4000  /* sanity cap for table sizes */

/* Early MOV control layouts. Revision 1 keeps the later audio chunk header. */
#define DF_MOV_CONTROL_REVISION      0x02
#define DF_MOV_CONTROL_REVISION_1    0x0001

#define DF_REV1_AUDIO_A_COUNT        0x1A
#define DF_REV1_AUDIO_B_COUNT        0x1C
#define DF_REV1_PLAYLIST_COUNT       0x34
#define DF_REV1_NEXT_CONTROL         0x36
#define DF_REV1_PLAYLIST_ORDER       0x83E
#define DF_REV1_PLAYLIST_LOOP        0x8BE

#define DF_PROTO_AUDIO_HEADER        0x02
#define DF_PROTO_DURATION_SAMPLES    370
#define DF_PROTO_WINDOWS_RATE        22050
#define DF_PROTO_MACINTOSH_RATE      22255  /* nearest integer to 0x56EE8BA3 (16.16) */
#define DF_PROTO_AUDIO_A_COUNT       0x02
#define DF_PROTO_AUDIO_B_COUNT       0x04
#define DF_PROTO_PLAYLIST_COUNT      0x1C
#define DF_PROTO_PLAYLIST_ORDER      0x822
#define DF_PROTO_PLAYLIST_MAX        64

/* .snd "container-0" variant (HELP.SND/KID.SND): unlike the trk/sfx/mov container-1 loop block, the
 * order list, track name and per-chunk names live in container 0. Offsets are relative to container 0.
 * The order list holds 1-based container ids; the chunk-name table has one Pascal entry per chunk
 * (entry k -> container k+1). */
#define DF_SND_ORDER_COUNT  0x22   /* u16 */
#define DF_SND_ORDER        0x26   /* u16[], 1-based container ids */
#define DF_SND_TRACK_NAME   0xa6   /* Pascal string */
#define DF_SND_CHUNK_NAMES  0xc2   /* Pascal entries, stride 0x18, one per chunk */
#define DF_SND_NAME_STRIDE  0x18

typedef struct {
    int container_id;
    int codec_flag;
    off_t offset;               /* absolute offset to compressed audio */
    int32_t size;               /* compressed size */
    int32_t sample_rate;
    int32_t uncompressed_size;
    int valid;
    char name[DF_MOV_NAME_MAX + 1];
} df_chunk_t;

typedef struct {
    char name[DF_MOV_NAME_MAX + 1];
} df_name_t;

typedef struct {
    int* sequence;              /* container ids in authored playback order */
    int count;
    int loop;
    int loop_start;             /* sequence index */
} df_early_playlist_t;

static VGMSTREAM* build_segmented(STREAMFILE* sf, df_chunk_t* chunks, int* seq, int count, int loop, int loop_start);

/* Read the audio fields of a container; returns 1 if it looks like a valid audio chunk. */
static bool is_valid_chunk(STREAMFILE* sf, int containers, int id, df_chunk_t* c) {
    if (id < 0 || id >= containers)
        return false;

    off_t pos = read_u32le(DF_HEADER_SIZE + id * 0x04, sf);
    if (pos <= 0 || pos >= get_streamfile_size(sf))
        return false;

    off_t hp = pos + 0x08; /* skip container header (id + size) */
    if (hp + 0x30 > get_streamfile_size(sf))
        return false;

    uint16_t codec = read_u16le(hp + 0x1A, sf);
    uint32_t rate  = read_u32le(hp + 0x1C, sf);
    if ((codec != 1 && codec != 2) || (rate != 11025 && rate != 22050 && rate != 44100))
        return false;

    uint32_t data_offset = read_u32le(hp + 0x2C, sf);

    c->container_id      = id;
    c->codec_flag        = codec;
    c->sample_rate       = rate;
    c->uncompressed_size = read_u32le(hp + 0x24, sf);
    c->offset            = hp + data_offset;
    c->size              = read_u32le(pos + 0x04, sf) - data_offset;
    c->valid             = 1;
    return true;
}

static int32_t count_v40_samples(STREAMFILE* sf, off_t offset, int32_t size) {
    if (size <= 0)
        return 0;

    off_t pos = offset + 1;
    off_t end = offset + size;
    int64_t samples = 1;

    while (pos < end) {
        uint8_t control = read_u8(pos++, sf);

        if ((control & 0x80) == 0) {
            samples++;
        }
        else if ((control & 0x40) == 0) {
            int count = (control & 0x3f) + 1;
            if (pos + count > end)
                return 0;
            pos += count;
            samples += count * 2;
        }
        else {
            samples += (control & 0x3f) + 1;
        }

        if (samples > INT32_MAX)
            return 0;
    }

    return (int32_t)samples;
}

/* Proto audio stores a native-endian u16 duration followed by raw v4.0 ADPCM. */
static bool is_valid_proto_chunk(STREAMFILE* sf, int containers, int id,
        read_u16_t read_u16, read_u32_t read_u32, int sample_rate, df_chunk_t* c) {
    off_t file_size = get_streamfile_size(sf);
    if (id <= 0 || id >= containers)
        return false;

    off_t pos = read_u32(DF_HEADER_SIZE + id * 0x04, sf);
    if (pos <= 0 || pos + 0x08 > file_size)
        return false;
    if (read_u32(pos, sf) != (uint32_t)id)
        return false;

    uint32_t payload_size = read_u32(pos + 0x04, sf);
    off_t hp = pos + 0x08;
    if (payload_size <= DF_PROTO_AUDIO_HEADER || payload_size > INT32_MAX || hp + payload_size > file_size)
        return false;

    off_t data_offset = hp + DF_PROTO_AUDIO_HEADER;
    int32_t data_size = payload_size - DF_PROTO_AUDIO_HEADER;
    int32_t samples = count_v40_samples(sf, data_offset, data_size);
    if (samples <= 0 || samples % DF_PROTO_DURATION_SAMPLES != 0)
        return false;

    int duration = samples / DF_PROTO_DURATION_SAMPLES;
    if (duration != read_u16(hp, sf))
        return false;

    c->container_id = id;
    c->codec_flag = 1;
    c->sample_rate = sample_rate;
    c->uncompressed_size = samples;
    c->offset = data_offset;
    c->size = data_size;
    c->valid = 1;
    return true;
}

static int32_t chunk_samples(df_chunk_t* c) {
    return (c->codec_flag == 1) ? c->uncompressed_size : c->uncompressed_size / 2;
}

static void config_chunk(VGMSTREAM* v, df_chunk_t* c) {
    v->meta_type   = meta_CF_DF;
    v->sample_rate = c->sample_rate;
    v->stream_size = c->size;
    if (c->codec_flag == 1) {
        v->coding_type = coding_CF_DF_ADPCM_V40;
        v->num_samples = c->uncompressed_size;
    } else {
        v->coding_type = coding_CF_DF_DPCM_V41;
        v->num_samples = c->uncompressed_size / 2;
    }
}

static bool is_silent_chunk(STREAMFILE* sf, const df_chunk_t* c) {
    uint8_t buf[0x1000];

    if (c->size <= 0)
        return true;

    off_t pos = c->offset;
    int32_t left = c->size;
    while (left > 0) {
        int to_read = left > (int)sizeof(buf) ? (int)sizeof(buf) : left;
        int got = read_streamfile(buf, pos, to_read, sf);
        if (got <= 0)
            return false; /* can't determine, (keep) */

        for (int i = 0; i < got; i++) {
            uint8_t b = buf[i];
            if (c->codec_flag == 2) {
                if (b != 0x00 && b != 0x80)
                    return false;
            } else {
                if (b != 0x40 && b < 0xC0)
                    return false;
            }
        }

        pos  += got;
        left -= got;
    }
    return true;
}

/* Pascal String (nameLen @ entry+0x0A, chars @ entry+0x0B). */
static void read_name(STREAMFILE* sf, off_t entry, int name_max, df_name_t* dst) {
    int len = read_u8(entry + DF_NAME_OFFSET, sf);
    if (len > name_max) {
        dst->name[0] = '\0';
        return;
    }
    for (int k = 0; k < len; k++)
        dst->name[k] = read_u8(entry + DF_NAME_OFFSET + 1 + k, sf);
    dst->name[len] = '\0';
}

static int get_mov_control_revision(STREAMFILE* sf) {
    off_t file_size = get_streamfile_size(sf);
    off_t pos = read_u32le(DF_HEADER_SIZE, sf);
    if (pos <= 0 || pos + 0x08 + DF_MOV_CONTROL_REVISION + 0x02 > file_size)
        return 0;
    return read_u16le(pos + 0x08 + DF_MOV_CONTROL_REVISION, sf);
}

static void trim_early_playlist(STREAMFILE* sf, df_chunk_t* chunks,
        const df_early_playlist_t* playlist, int* out_lo, int* out_hi,
        int* out_loop, int* out_loop_start) {
    int lo = 0, hi = playlist->count - 1;
    int loop = playlist->loop;
    int loop_start = playlist->loop_start;

    /* Trim non-interspaced silence */
    while (lo <= hi && is_silent_chunk(sf, &chunks[playlist->sequence[lo]]))
        lo++;
    while (hi >= lo && is_silent_chunk(sf, &chunks[playlist->sequence[hi]]))
        hi--;
    if (lo > hi) {
        lo = 0;
        hi = playlist->count - 1;
    }

    if (loop) {
        if (loop_start > hi) {
            loop = 0;
            loop_start = 0;
        }
        else {
            if (loop_start < lo)
                loop_start = lo;
            loop_start -= lo;
        }
    }

    *out_lo = lo;
    *out_hi = hi;
    *out_loop = loop;
    *out_loop_start = loop_start;
}

static void set_stream_name(STREAMFILE* sf, VGMSTREAM* vgmstream,
        const char* authored_name, int stream_index) {
    if (authored_name && authored_name[0]) {
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", authored_name);
    }
    else {
        char basename[STREAM_NAME_SIZE];
        get_streamfile_filename(sf, basename, sizeof(basename));
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%.*s#%d",
                STREAM_NAME_SIZE - (11 + 1 + 1), basename, stream_index);
    }
}

/* Revision-1 MOVs keep Group-A one-shots and Group-B background sources
 * immediately after each control. */
static VGMSTREAM* build_revision1_mov(STREAMFILE* sf, int containers) {
    VGMSTREAM* vgmstream = NULL;
    df_chunk_t* chunks = NULL;
    int* audio_ids = NULL;
    df_early_playlist_t playlist = {0};
    int audio_count = 0;
    int control = 0;
    bool has_playlist = false;

    chunks = calloc((size_t)containers, sizeof(*chunks));
    audio_ids = malloc((size_t)containers * sizeof(*audio_ids));
    if (!chunks || !audio_ids)
        goto fail;

    off_t file_size = get_streamfile_size(sf);
    while (control < containers) {
        off_t pos = read_u32le(DF_HEADER_SIZE + (off_t)control * 0x04, sf);
        if (pos <= 0 || pos + 0x08 > file_size)
            goto fail;
        off_t p = pos + 0x08;

        int audio_a, audio_b, next;
        int order_count;
        off_t order_pos;
        int loop = 0, loop_start = 0;

        if (p + DF_REV1_PLAYLIST_LOOP + 0x04 > file_size)
            goto fail;
        if (read_u16le(p + DF_MOV_CONTROL_REVISION, sf) != DF_MOV_CONTROL_REVISION_1)
            goto fail;
        audio_a = read_u16le(p + DF_REV1_AUDIO_A_COUNT, sf);
        audio_b = read_u16le(p + DF_REV1_AUDIO_B_COUNT, sf);
        order_count = read_u16le(p + DF_REV1_PLAYLIST_COUNT, sf);
        order_pos = p + DF_REV1_PLAYLIST_ORDER;
        uint32_t authored_loop = read_u32le(p + DF_REV1_PLAYLIST_LOOP, sf);
        if (authored_loop < (uint32_t)order_count) {
            loop = 1;
            loop_start = authored_loop;
        }
        uint32_t authored_next = read_u32le(p + DF_REV1_NEXT_CONTROL, sf);
        if (authored_next > (uint32_t)containers)
            goto fail;
        next = authored_next ? (int)authored_next : containers;

        int audio_base = control + 1;
        int group_b_base = audio_base + audio_a;
        if (audio_a + audio_b > containers || audio_base + audio_a + audio_b > containers)
            goto fail;
        if (next <= control || next > containers)
            goto fail;
        if (order_count > DF_MAX_CHUNKS)
            goto fail;
        if (order_pos + (off_t)order_count * 0x02 > file_size)
            goto fail;

        for (int i = 0; i < audio_a + audio_b; i++) {
            int id = audio_base + i;
            if (!is_valid_chunk(sf, containers, id, &chunks[id]))
                goto fail;
            if (audio_count >= containers)
                goto fail;
            audio_ids[audio_count++] = id;
        }

        if (audio_b > 0 && order_count > 0) {
            int old_count = playlist.count;

            for (int i = 0; i < order_count; i++) {
                int selector = read_u16le(order_pos + (off_t)i * 0x02, sf);
                if (selector < 1 || selector > audio_b)
                    goto fail;
            }

            int* sequence = realloc(playlist.sequence,
                    (size_t)(old_count + order_count) * sizeof(*sequence));
            if (!sequence)
                goto fail;
            playlist.sequence = sequence;

            for (int i = 0; i < order_count; i++) {
                int selector = read_u16le(order_pos + (off_t)i * 0x02, sf);
                playlist.sequence[old_count + i] = group_b_base + selector - 1;
            }
            playlist.count = old_count + order_count;

            /* One output track has one loop point. Keep the first authored
             * point when a MOV contributes more than one control sequence. */
            if (loop && !playlist.loop) {
                playlist.loop = 1;
                playlist.loop_start = old_count + loop_start;
            }
            has_playlist = true;
        }

        if (next == containers)
            break;
        control = next;
    }

    int subsongs = (has_playlist ? 1 : 0) + audio_count;
    if (subsongs <= 0)
        goto fail;

    int target = sf->stream_index;
    if (target == 0)
        target = 1;
    if (target < 1 || target > subsongs)
        goto fail;

    if (has_playlist && target == 1) {
        int lo = 0, hi = playlist.count - 1;
        int loop = playlist.loop;
        int loop_start = playlist.loop_start;

        trim_early_playlist(sf, chunks, &playlist, &lo, &hi, &loop, &loop_start);

        vgmstream = build_segmented(sf, chunks, playlist.sequence + lo,
                hi - lo + 1, loop, loop_start);
        if (!vgmstream)
            goto fail;
    }
    else {
        int piece = target - (has_playlist ? 1 : 0);
        int id = audio_ids[piece - 1];
        vgmstream = allocate_vgmstream(1, 0);
        if (!vgmstream)
            goto fail;
        config_chunk(vgmstream, &chunks[id]);
        if (!vgmstream_open_stream(vgmstream, sf, chunks[id].offset))
            goto fail;
    }

    set_stream_name(sf, vgmstream, NULL, target);
    vgmstream->num_streams = subsongs;
    free(chunks);
    free(audio_ids);
    free(playlist.sequence);
    return vgmstream;

fail:
    free(chunks);
    free(audio_ids);
    free(playlist.sequence);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Proto resources serialize the complete container in the platform's native byte order. */
static bool get_proto_config(STREAMFILE* sf, int* out_containers, bool* out_big_endian) {
    off_t file_size = get_streamfile_size(sf);
    if (file_size <= 0 || file_size > UINT32_MAX)
        return false;
    if (is_id32be(0x20, sf, "LPPA") && is_id32be(0x24, sf, "LPPA"))
        return false;

    bool valid_le = read_u32le(0x00, sf) == 0x00010000 &&
                    read_u32le(0x04, sf) == (uint32_t)file_size;
    bool valid_be = read_u32be(0x00, sf) == 0x00010000 &&
                    read_u32be(0x04, sf) == (uint32_t)file_size;
    if (valid_le == valid_be)
        return false;

    bool big_endian = valid_be;
    read_u32_t read_u32 = get_read_u32(big_endian);
    uint32_t containers = read_u32(0x14, sf);
    if (containers <= 1 || containers > INT16_MAX)
        return false;
    if (DF_HEADER_SIZE + (off_t)containers * 0x04 > file_size)
        return false;

    *out_containers = containers;
    *out_big_endian = big_endian;
    return true;
}

/* Proto containers intersperse sound with non-sound resources.
 * MOV resources may additionally carry a complete Group-B playlist in control 0; */
static VGMSTREAM* build_proto_resource(STREAMFILE* sf, int containers,
        bool big_endian, bool parse_movie_playlist) {
    VGMSTREAM* vgmstream = NULL;
    df_chunk_t* chunks = NULL;
    int* audio_ids = NULL;
    df_early_playlist_t playlist = {0};
    read_u16_t read_u16 = get_read_u16(big_endian);
    read_u32_t read_u32 = get_read_u32(big_endian);
    int sample_rate = big_endian ? DF_PROTO_MACINTOSH_RATE : DF_PROTO_WINDOWS_RATE;
    int audio_count = 0;
    bool has_playlist = false;

    chunks = calloc((size_t)containers, sizeof(*chunks));
    audio_ids = malloc((size_t)containers * sizeof(*audio_ids));
    if (!chunks || !audio_ids)
        goto fail;

    for (int id = 1; id < containers; id++) {
        if (is_valid_proto_chunk(sf, containers, id, read_u16, read_u32,
                sample_rate, &chunks[id])) {
            audio_ids[audio_count++] = id;
        }
    }
    if (audio_count <= 0)
        goto fail;

    if (parse_movie_playlist) {
        /* Proto MOV has one control record. Group-B selectors are 1-based and
         * loop from entry zero. */
        off_t file_size = get_streamfile_size(sf);
        off_t control = read_u32(DF_HEADER_SIZE, sf);
        if (control > 0 && control + 0x08 <= file_size &&
            read_u32(control, sf) == 0) {
            uint32_t payload_size = read_u32(control + 0x04, sf);
            off_t p = control + 0x08;

            if (payload_size <= file_size - p &&
                payload_size >= DF_PROTO_PLAYLIST_COUNT + 0x02) {
                int audio_a = read_u16(p + DF_PROTO_AUDIO_A_COUNT, sf);
                int audio_b = read_u16(p + DF_PROTO_AUDIO_B_COUNT, sf);
                int order_count = read_u16(p + DF_PROTO_PLAYLIST_COUNT, sf);

                if (audio_b > 0 && order_count > 0 &&
                    order_count <= DF_PROTO_PLAYLIST_MAX &&
                    audio_a + audio_b < containers &&
                    DF_PROTO_PLAYLIST_ORDER + (off_t)order_count * 0x02 <= payload_size) {
                    bool valid = true;

                    playlist.sequence = malloc((size_t)order_count * sizeof(*playlist.sequence));
                    if (!playlist.sequence)
                        goto fail;

                    for (int i = 0; i < order_count; i++) {
                        int selector = read_u16(p + DF_PROTO_PLAYLIST_ORDER + (off_t)i * 0x02, sf);
                        int id = audio_a + selector;

                        if (selector < 1 || selector > audio_b || !chunks[id].valid) {
                            valid = false;
                            break;
                        }
                        playlist.sequence[i] = id;
                    }

                    if (valid) {
                        playlist.count = order_count;
                        playlist.loop = 1;
                        playlist.loop_start = 0;
                        has_playlist = true;
                    }
                    else {
                        free(playlist.sequence);
                        playlist.sequence = NULL;
                    }
                }
            }
        }
    }

    int subsongs = audio_count + (has_playlist ? 1 : 0);
    int target = sf->stream_index;
    if (target == 0)
        target = 1;
    if (target < 1 || target > subsongs)
        goto fail;

    if (has_playlist && target == 1) {
        int lo, hi, loop, loop_start;

        trim_early_playlist(sf, chunks, &playlist, &lo, &hi, &loop, &loop_start);
        vgmstream = build_segmented(sf, chunks, playlist.sequence + lo,
                hi - lo + 1, loop, loop_start);
        if (!vgmstream)
            goto fail;
    }
    else {
        int piece = target - (has_playlist ? 1 : 0);
        int id = audio_ids[piece - 1];

        vgmstream = allocate_vgmstream(1, 0);
        if (!vgmstream)
            goto fail;

        config_chunk(vgmstream, &chunks[id]);
        if (!vgmstream_open_stream(vgmstream, sf, chunks[id].offset))
            goto fail;
    }

    set_stream_name(sf, vgmstream, NULL, target);
    vgmstream->num_streams = subsongs;
    free(chunks);
    free(audio_ids);
    free(playlist.sequence);
    return vgmstream;

fail:
    free(chunks);
    free(audio_ids);
    free(playlist.sequence);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Assembled track via segmented layout: play the given sequence of loop chunks once.
 * loop=1 marks the whole assembled track as an end-to-end loop (.snd/.sfx/.trk only; .mov excluded). */
static VGMSTREAM* build_segmented(STREAMFILE* sf, df_chunk_t* chunks, int* seq, int count, int loop, int loop_start) {
    VGMSTREAM* v = NULL;
    segmented_layout_data* data = init_layout_segmented(count);
    if (!data) goto fail;

    for (int i = 0; i < count; i++) {
        VGMSTREAM* seg = allocate_vgmstream(1, 0);
        if (!seg) goto fail;
        data->segments[i] = seg;

        config_chunk(seg, &chunks[seq[i]]);
        if (!vgmstream_open_stream(seg, sf, chunks[seq[i]].offset))
            goto fail;
    }

    if (!setup_layout_segmented(data))
        goto fail;

    v = allocate_segmented_vgmstream(data, loop, loop_start, count - 1);
    if (!v) goto fail;
    return v;

fail:
    free_layout_segmented(data);
    close_vgmstream(v);
    return NULL;
}

/* Assembled track via blocked layout: one continuous stream traversing the loop list (disk-stream
 * mov, where blocks are scattered among video frames). */
static VGMSTREAM* build_blocked(STREAMFILE* sf, df_chunk_t* loop, int loop_count, off_t first_entry) {
    VGMSTREAM* v = allocate_vgmstream(1, 0);
    if (!v) return NULL;
    /* CF_DF silence trim: drop leading/trailing fully-silent blocks but keep interspersed silence
     * (gaps with audio on both sides). Leading is handled by priming at entry[lo]; trailing by
     * limiting num_samples to [lo..hi] (the renderer stops there and never reaches the tail).
     * To restore verbatim block walking, force lo=0 / hi=loop_count-1. */
    int lo = 0, hi = loop_count - 1;
    while (lo <= hi && is_silent_chunk(sf, &loop[lo]))
        lo++;
    while (hi >= lo && is_silent_chunk(sf, &loop[hi]))
        hi--;
    if (lo > hi) { /* degenerate all-silent track: keep everything */
        lo = 0;
        hi = loop_count - 1;
    }

    int32_t total = 0, max_rate = 0;
    for (int i = lo; i <= hi; i++) {
        total += chunk_samples(&loop[i]);
        if (loop[i].sample_rate > max_rate)
            max_rate = loop[i].sample_rate;
    }

    v->meta_type    = meta_CF_DF;
    v->coding_type  = (loop[lo].codec_flag == 1) ? coding_CF_DF_ADPCM_V40 : coding_CF_DF_DPCM_V41;
    v->sample_rate  = max_rate;
    v->num_samples  = total;
    v->layout_type  = layout_blocked_cf_df;

    /* prime entry[lo] so leading silent blocks are skipped (layout re-derives index from the file) */
    if (!vgmstream_open_stream(v, sf, first_entry + (off_t)lo * DF_LOOP_ENTRY)) {
        close_vgmstream(v);
        return NULL;
    }
    return v;
}

/* DreamFactory v4 .snd "container-0" variant: the assembled theme's order list, track name and
 * per-chunk names live in container 0, unlike the container 1 found in trk/sfx/mov.
 * Builds the theme (subsong 1: order assembled in order, leading/trailing silence trimmed,
 * end-to-end loop) plus every chunk as a named subsong. *handled is set once a valid container-0
 * order is recognized, so the caller stops instead of falling through to the container-1 path. */
static VGMSTREAM* build_snd_c0(STREAMFILE* sf, int containers, int* handled) {
    VGMSTREAM* vgmstream = NULL;
    df_chunk_t* chunks = NULL;
    int* seq = NULL;
    int* listed = NULL;
    df_name_t* names = NULL;
    char track_name[STREAM_NAME_SIZE];
    int order_count, listed_count = 0, subsongs, target;
    bool has_track;
    off_t fsize, c0;

    *handled = 0;
    track_name[0] = '\0';
    fsize = get_streamfile_size(sf);

    c0 = read_u32le(DF_HEADER_SIZE + 0x00 * 0x04, sf);
    if (c0 <= 0 || c0 + DF_SND_CHUNK_NAMES > fsize)
        return NULL;
    order_count = read_u16le(c0 + DF_SND_ORDER_COUNT, sf);
    if (order_count <= 0 || order_count > DF_MAX_CHUNKS)
        return NULL; /* no container-0 order -> not this variant; caller falls through */
    if (c0 + DF_SND_ORDER + (off_t)order_count * 0x02 > fsize)
        return NULL;

    *handled = 1; /* a valid container-0 .snd from here -> NULL means parse failure, do not fall through */

    chunks = calloc((size_t)containers, sizeof(df_chunk_t));
    names  = calloc((size_t)containers, sizeof(df_name_t));
    seq    = malloc((size_t)order_count * sizeof(int));
    listed = malloc((size_t)containers * sizeof(int));
    if (!chunks || !names || !seq || !listed)
        goto fail;

    for (int i = 0; i < containers; i++)
        is_valid_chunk(sf, containers, i, &chunks[i]); /* fills chunks[i], sets .valid */

    /* order list -> seq (1-based container ids); bail if any reference isn't a real audio chunk */
    for (int i = 0; i < order_count; i++) {
        int id = read_u16le(c0 + DF_SND_ORDER + (off_t)i * 0x02, sf);
        if (id < 1 || id >= containers || !chunks[id].valid)
            goto fail;
        seq[i] = id;
    }

    /* per-chunk names: entry k (0-based) -> container k+1 */
    for (int id = 1; id < containers; id++) {
        off_t e = c0 + DF_SND_CHUNK_NAMES + (off_t)(id - 1) * DF_SND_NAME_STRIDE;
        if (e + 1 > fsize)
            break;
        int ln = read_u8(e, sf);
        if (ln <= 0 || ln > DF_SHORT_NAME_MAX || e + 1 + ln > fsize)
            continue;
        for (int k = 0; k < ln; k++)
            names[id].name[k] = read_u8(e + 1 + k, sf);
        names[id].name[ln] = '\0';
    }

    /* track name */
    {
        int tl = read_u8(c0 + DF_SND_TRACK_NAME, sf);
        if (tl > 0 && tl <= DF_MOV_NAME_MAX && c0 + DF_SND_TRACK_NAME + 1 + tl <= fsize) {
            for (int k = 0; k < tl; k++)
                track_name[k] = read_u8(c0 + DF_SND_TRACK_NAME + 1 + k, sf);
            track_name[tl] = '\0';
        }
    }

    /* individual list: every valid audio chunk, kept (not folded) so each stays its own named subsong */
    for (int i = 0; i < containers; i++)
        if (chunks[i].valid)
            listed[listed_count++] = i;

    has_track = (order_count > 1); /* a single-step order is not a real assembled track */
    subsongs = (has_track ? 1 : 0) + listed_count;
    if (subsongs == 0)
        goto fail;

    target = sf->stream_index;
    if (target == 0)
        target = 1;
    if (target < 0 || target > subsongs)
        goto fail;

    if (has_track && target == 1) {
        /* subsong 1: assembled theme, leading/trailing silence trimmed, looped end-to-end */
        int lo = 0, hi = order_count - 1;
        while (lo <= hi && is_silent_chunk(sf, &chunks[seq[lo]]))
            lo++;
        while (hi >= lo && is_silent_chunk(sf, &chunks[seq[hi]]))
            hi--;
        if (lo > hi) { lo = 0; hi = order_count - 1; } /* all silent: keep so it isn't empty */

        vgmstream = build_segmented(sf, chunks, seq + lo, hi - lo + 1, 1, 0);
        if (!vgmstream)
            goto fail;
        set_stream_name(sf, vgmstream, track_name, target);
    }
    else {
        int piece = target - (has_track ? 1 : 0); /* 1-based index into listed[] */
        int id = listed[piece - 1];

        vgmstream = allocate_vgmstream(1, 0);
        if (!vgmstream)
            goto fail;
        config_chunk(vgmstream, &chunks[id]);
        set_stream_name(sf, vgmstream, names[id].name, target);
        if (!vgmstream_open_stream(vgmstream, sf, chunks[id].offset))
            goto fail;
    }

    vgmstream->num_streams = subsongs;
    free(chunks);
    free(names);
    free(seq);
    free(listed);
    return vgmstream;

fail:
    free(chunks);
    free(names);
    free(seq);
    free(listed);
    close_vgmstream(vgmstream);
    return NULL;
}

VGMSTREAM* init_vgmstream_cf_df(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    df_name_t* names = NULL;
    int* audio_ids = NULL;
    df_chunk_t* loop = NULL;
    int16_t* order = NULL;
    int* seq = NULL;
    uint8_t* in_loop = NULL;
    char track_name[STREAM_NAME_SIZE];

    if (!check_extensions(sf, "snd,sfx,trk,mov,move"))
        return NULL;

    int is_mov = check_extensions(sf, "mov,move");
    if (is_mov) {
        int proto_containers;
        bool proto_big_endian;
        if (get_proto_config(sf, &proto_containers, &proto_big_endian))
            return build_proto_resource(sf, proto_containers, proto_big_endian, is_mov);
    }

    if (read_u32le(0x04, sf) != get_streamfile_size(sf))
        return NULL;
    int containers = read_u32le(0x14, sf);
    if (containers <= 0 || containers > INT16_MAX)
        return NULL;
    if (DF_HEADER_SIZE + (off_t)containers * 0x04 > get_streamfile_size(sf))
        return NULL;

    int has_lppa = is_id32be(0x20, sf, "LPPA") && is_id32be(0x24, sf, "LPPA");
    if (is_mov && get_mov_control_revision(sf) == DF_MOV_CONTROL_REVISION_1)
        return build_revision1_mov(sf, containers);
    if (!has_lppa)
        return NULL;

    /* Most .snd files store the theme in container 0.
     * A few rare .snd files may instead use a container 1 loop block
     * and instead fall through to the shared path below. */
    if (check_extensions(sf, "snd")) {
        int handled = 0;
        vgmstream = build_snd_c0(sf, containers, &handled);
        if (handled)
            return vgmstream;
    }

    int loop_track = check_extensions(sf, "sfx,snd,trk");
    int target = sf->stream_index;
    if (target == 0)
        target = 1;
    track_name[0] = '\0';

    // definitions before 'goto fail'
    int loop_count, order_count;
    off_t first_entry, c1, c0;
    int audio_count, disk_stream;
    int has_loop, subsongs;

    names = calloc(containers, sizeof(*names));
    audio_ids = malloc(containers * sizeof(int));
    if (!names || !audio_ids)
        goto fail;

    /* --- loop block (container 1): chunkOrder + loop chunk list --- */
    loop_count = 0;
    order_count = 0;
    first_entry = 0;
    c1 = read_u32le(DF_HEADER_SIZE + 0x01 * 0x04, sf);
    if (c1 > 0) {
        off_t lp = c1 + 0x08;

        order_count = read_u16le(lp + 0x04, sf);
        if (order_count < 0 || order_count > DF_MAX_CHUNKS)
            order_count = 0;
        if (order_count > 0) {
            order = malloc(order_count * sizeof(int16_t));
            if (!order) goto fail;
            for (int i = 0; i < order_count; i++)
                order[i] = read_s16le(lp + 0x06 + i * 0x02, sf);
        }

        off_t list_pos = lp + 0x06 + DF_ORDER_REGION;
        int lc = read_u16le(list_pos, sf);
        if (lc > 0 && lc <= containers) {
            first_entry = list_pos + 0x04;
            loop = calloc(lc, sizeof(df_chunk_t));
            if (!loop) goto fail;

            int all_valid = 1;
            off_t e = first_entry;
            for (int i = 0; i < lc; i++) {
                int id = read_u16le(e + 0x04, sf);
                if (id >= 0 && id < containers)
                    read_name(sf, e, DF_SHORT_NAME_MAX, &names[id]);
                if (!is_valid_chunk(sf, containers, id, &loop[i]))
                    all_valid = 0;
                e += DF_LOOP_ENTRY;
            }
            if (all_valid)
                loop_count = lc; /* only assemble if every referenced chunk is valid audio */
        }
    }

    /* Disk-stream movies  carry loop_count > order_count fragments; subsong 1 assembles
     * them via the blocked layout. Mark loop-member containers so they can be dropped from the
     * individual piece list
     */
    disk_stream = (loop_count > order_count);
    if (loop_count > 0 && disk_stream) {
        in_loop = calloc(containers, 1);
        if (!in_loop) goto fail;
        for (int i = 0; i < loop_count; i++)
            in_loop[loop[i].container_id] = 1;
    }

    /* --- single block (named one-shots) + non-MOV track name --- */
    c0 = read_u32le(DF_HEADER_SIZE + 0x00 * 0x04, sf);
    if (c0 > 0) {
        off_t p0 = c0 + 0x08;

        if (!is_mov) {
            int tl = read_u8(p0 + 0x24, sf);
            if (tl > 0 && tl <= DF_MOV_NAME_MAX) {
                for (int k = 0; k < tl; k++)
                    track_name[k] = read_u8(p0 + 0x25 + k, sf);
                track_name[tl] = '\0';
            }
        }

        int single_idx = read_u32le(p0 + (is_mov ? 0x60 : 0x20), sf);
        if (single_idx > 0 && single_idx < containers) {
            off_t sp = read_u32le(DF_HEADER_SIZE + single_idx * 0x04, sf);
            if (sp > 0) {
                off_t spp = sp + 0x08;
                int count = read_u16le(spp + 0x04, sf);
                int name_max = is_mov ? DF_MOV_NAME_MAX : DF_SHORT_NAME_MAX;
                int stride = is_mov ? DF_MOV_ENTRY : DF_LOOP_ENTRY;
                if (count > 0 && count <= containers) {
                    off_t e = spp + 0x08;
                    for (int i = 0; i < count; i++) {
                        int id = read_u32le(e + 0x04, sf);
                        if (id >= 0 && id < containers)
                            read_name(sf, e, name_max, &names[id]);
                        e += stride;
                    }
                }
            }
        }
    }

    audio_count = 0;
    for (int i = 0; i < containers; i++) {
        if (in_loop && in_loop[i])
            continue;
        df_chunk_t tmp;
        if (is_valid_chunk(sf, containers, i, &tmp))
            audio_ids[audio_count++] = i;
    }

    has_loop = (loop_count > 0);
    subsongs = (has_loop ? 1 : 0) + audio_count;
    if (subsongs == 0)
        goto fail;
    if (target < 0 || target > subsongs)
        goto fail;

    if (has_loop && target == 1) {
        /* subsong 1: the assembled track */
        if (disk_stream && loop[0].codec_flag == 2) {
            /* scattered stream of v4.1 blocks -> blocked layout */
            vgmstream = build_blocked(sf, loop, loop_count, first_entry);
        } else {
            /* follow chunkOrder if usable, else play loop chunks in list order */
            int use_order = (order_count > 0);
            for (int i = 0; use_order && i < order_count; i++) {
                int idx = order[i] - 1;
                if (idx < 0 || idx >= loop_count)
                    use_order = 0;
            }
            int count = use_order ? order_count : loop_count;
            seq = malloc(count * sizeof(int));
            if (!seq) goto fail;
            for (int i = 0; i < count; i++)
                seq[i] = use_order ? (order[i] - 1) : i;
            /* CF_DF silence trim: drop leading/trailing fully-silent steps (the LOGO/OCREDITS
             * cascades are silent chunks repeated at the ends) but keep interspersed silence.
             * Force lo=0 / hi=count-1 to disable. */
            int lo = 0, hi = count - 1;
            while (lo <= hi && is_silent_chunk(sf, &loop[seq[lo]]))
                lo++;
            while (hi >= lo && is_silent_chunk(sf, &loop[seq[hi]]))
                hi--;
            if (lo > hi) { /* degenerate all-silent track: keep unfiltered so subsong 1 isn't empty */
                lo = 0;
                hi = count - 1;
            }
            vgmstream = build_segmented(sf, loop, seq + lo, hi - lo + 1, loop_track, 0);
        }
        if (!vgmstream)
            goto fail;

        set_stream_name(sf, vgmstream,
                !is_mov ? track_name : NULL, target);
    } else {
        /* subsongs 2..N (or 1..N when no loop block): individual audio pieces */
        int piece = target - (has_loop ? 1 : 0);
        int id = audio_ids[piece - 1];
        df_chunk_t c;

        if (!is_valid_chunk(sf, containers, id, &c))
            goto fail;

        vgmstream = allocate_vgmstream(1, 0);
        if (!vgmstream)
            goto fail;

        config_chunk(vgmstream, &c);
        set_stream_name(sf, vgmstream, names[id].name, target);

        if (!vgmstream_open_stream(vgmstream, sf, c.offset))
            goto fail;
    }

    vgmstream->num_streams = subsongs;

    free(names);
    free(audio_ids);
    free(loop);
    free(order);
    free(seq);
    free(in_loop);
    return vgmstream;

fail:
    free(names);
    free(audio_ids);
    free(loop);
    free(order);
    free(seq);
    free(in_loop);
    close_vgmstream(vgmstream);
    return NULL;
}
