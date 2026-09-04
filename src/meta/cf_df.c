#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

/*
 * CF_DF - CyberFlix DreamFactory Engine
 *
 * Generations:
 * - Proto (Luncius/Jump Raven): Native-endian containers store a duration followed by raw v4.0 ADPCM.
 * - V1 (Dust 1996): Control records divide sounds into one-shot and reusable
 *   background groups selected by authored playlists.
 * - V4 (TAOOT - DMQwA/DRQwA): LPPALPPA containers use explicit loop and
 *   one-shot blocks; some .snd files reference their loop block from control 0.
 *
 * Shared structure:
 * - 0x400-byte header: file size @0x04, container count @0x14; later files use
 *   "LPPALPPA" @0x20.
 * - u32 offset table @0x400 pointing to each container (u32 id, u32 size, then payload).
 * - Later audio chunks hold: u16 codec @0x1A (1=v4.0 / 2=v4.1), u32 rate @0x1C,
 *   u32 uncompressed size @0x24, u32 data offset @0x2C.
 * Format-specific control data describes a playback track plus stored one-shot
 * and reusable background chunks. subsong 1 = the assembled background track,
 * subsongs 2..N = every stored audio chunk individually.
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
#define DF_CODEC_V40    0x01
#define DF_CODEC_V41    0x02

/* Early MOV control layouts. Revision 1 keeps the later audio chunk header. */
#define DF_MOV_CONTROL_REVISION_1    (1u << 16)

#define DF_PROTO_AUDIO_HEADER        0x02
#define DF_PROTO_DURATION_SAMPLES    370
#define DF_PROTO_WINDOWS_RATE        22050
#define DF_PROTO_MACINTOSH_RATE      22255  /* nearest integer to 0x56EE8BA3 (16.16) */
#define DF_PROTO_PLAYLIST_MAX        64

/* Revision-4 .snd control payload. Unlike the shared .mov/.sfx/.trk/.11k layout,
 * the loop block is referenced by container id instead of fixed at id 1. */
#define DF_SND_REVISION              0x02
#define DF_SND_REVISION_4            0x0004
#define DF_SND_LOOP_BLOCK_PTR        0x1C

#define DF_SND_ORDER_MAX         64
#define DF_SND_NAME_STRIDE  0x18

typedef struct {
    int container_id;
    int codec_flag;
    off_t offset;               /* absolute offset to compressed audio */
    int32_t size;               /* compressed size */
    int32_t sample_rate;
    int32_t uncompressed_size;
    bool valid;
} df_chunk_t;

typedef struct {
    char name[DF_MOV_NAME_MAX + 1];
} df_name_t;

typedef enum {
    DF_VERSION_NONE = 0,
    DF_VERSION_PROTO,
    DF_VERSION_1,
    DF_VERSION_4,
} df_version_t;

typedef struct {
    int* sequence;              /* container ids in authored playback order */
    int count;
    int loop;
    int loop_start;             /* sequence index */
} df_playlist_t;

typedef struct {
    df_chunk_t* chunks;         /* indexed by container id */
    int* audio_ids;             /* individual subsongs in presentation order */
    int audio_count;
    df_playlist_t playlist;     /* single assembled background subsong */
    bool has_playlist;
    df_name_t* names;           /* optional names indexed by container id */
    char track_name[STREAM_NAME_SIZE];
    int containers;
} df_bank_t;

typedef struct {
    int audio_a_count;
    int audio_b_count;
    int order_count;
    off_t order;
    int next_control;
    int loop;
    int loop_start;
} df_mov_control_t;

typedef struct {
    int group_a_boundary;
    int group_b_count;
    int order_count;
    off_t order;
} df_bank_control_t;

/* .snd "container-0" variant: unlike the trk/sfx/mov
 * container-1 loop block, the order list and names live in container 0. */
typedef struct {
    df_bank_control_t bank;
    off_t track_name;
    off_t chunk_names;
} df_v1_snd_config_t;

static VGMSTREAM* build_segmented(STREAMFILE* sf, df_chunk_t* chunks, const int* seq, int count, int loop, int loop_start);

/* Read the audio fields of a container; returns 1 if it looks like a valid audio chunk. */
static bool is_valid_chunk(STREAMFILE* sf, int containers, int id,
        read_u16_t read_u16, read_u32_t read_u32, df_chunk_t* c) {
    if (id < 0 || id >= containers)
        return false;

    off_t pos = read_u32(DF_HEADER_SIZE + id * 0x04, sf);
    if (pos <= 0 || pos >= get_streamfile_size(sf))
        return false;

    off_t hp = pos + 0x08; /* skip container header (id + size) */
    if (hp + 0x30 > get_streamfile_size(sf))
        return false;

    uint16_t codec = read_u16(hp + 0x1A, sf);
    uint32_t rate  = read_u32(hp + 0x1C, sf);
    if ((codec != DF_CODEC_V40 && codec != DF_CODEC_V41) ||
            (rate != 11025 && rate != 22050 && rate != 44100))
        return false;

    uint32_t data_offset = read_u32(hp + 0x2C, sf);

    c->container_id      = id;
    c->codec_flag        = codec;
    c->sample_rate       = rate;
    c->uncompressed_size = read_u32(hp + 0x24, sf);
    c->offset            = hp + data_offset;
    c->size              = read_u32(pos + 0x04, sf) - data_offset;
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
    c->codec_flag = DF_CODEC_V40;
    c->sample_rate = sample_rate;
    c->uncompressed_size = samples;
    c->offset = data_offset;
    c->size = data_size;
    c->valid = 1;
    return true;
}

static int32_t chunk_samples(const df_chunk_t* c) {
    return (c->codec_flag == DF_CODEC_V40) ? c->uncompressed_size : c->uncompressed_size / 2;
}

static void config_chunk(VGMSTREAM* v, const df_chunk_t* c) {
    v->meta_type   = meta_CF_DF;
    v->sample_rate = c->sample_rate;
    v->stream_size = c->size;
    if (c->codec_flag == DF_CODEC_V40) {
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
            if (c->codec_flag == DF_CODEC_V41) {
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

static bool has_v1_mov_control(STREAMFILE* sf, read_u32_t read_u32) {
    off_t file_size = get_streamfile_size(sf);
    off_t pos = read_u32(DF_HEADER_SIZE, sf);
    if (pos <= 0 || pos + 0x08 + 0x04 > file_size)
        return false;
    return read_u32(pos + 0x08, sf) == DF_MOV_CONTROL_REVISION_1;
}

/* Revision-4 .snd stores the normal loop block's container id in control 0.
 * Other generations use different data at this offset, so the revision gate is
 * required before interpreting it as a pointer. */
static int get_snd_loop_block_id(STREAMFILE* sf, int containers) {
    off_t file_size = get_streamfile_size(sf);
    off_t pos = read_u32le(DF_HEADER_SIZE, sf);

    if (pos <= 0 || pos + 0x08 > file_size)
        return 1;
    if (read_u32le(pos, sf) != 0)
        return 1;

    uint32_t payload_size = read_u32le(pos + 0x04, sf);
    off_t p = pos + 0x08;
    if (payload_size < DF_SND_LOOP_BLOCK_PTR + 0x04 || payload_size > file_size - p)
        return 1;
    if (read_u16le(p + DF_SND_REVISION, sf) != DF_SND_REVISION_4)
        return 1;

    uint32_t id = read_u32le(p + DF_SND_LOOP_BLOCK_PTR, sf);
    if (id == 0 || id >= (uint32_t)containers)
        return 0;

    off_t block = read_u32le(DF_HEADER_SIZE + (off_t)id * 0x04, sf);
    if (block <= 0 || block + 0x08 > file_size)
        return 0;
    if (read_u32le(block, sf) != id)
        return 0;

    uint32_t block_size = read_u32le(block + 0x04, sf);
    if (block_size < 0x06 + DF_ORDER_REGION + 0x04 || block_size > file_size - (block + 0x08))
        return 0;

    return id;
}

static void trim_playlist(STREAMFILE* sf, df_chunk_t* chunks,
        const df_playlist_t* playlist, int* out_lo, int* out_hi,
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

/* Older generations select a contiguous Group-B range with one-based values.
 * Returns 1 when populated, 0 for invalid control data and -1 on allocation failure. */
static int append_contiguous_playlist(STREAMFILE* sf, read_u16_t read_u16,
        df_chunk_t* chunks, int containers, int group_boundary, int group_count,
        off_t order_offset, int order_count, int loop, int loop_start,
        df_playlist_t* playlist) {
    int old_count = playlist->count;

    if (group_boundary < 0 || group_count <= 0 || order_count <= 0 ||
            group_boundary + group_count >= containers)
        return 0;

    for (int i = 0; i < order_count; i++) {
        int selector = read_u16(order_offset + (off_t)i * 0x02, sf);
        int id = group_boundary + selector;
        if (selector < 1 || selector > group_count || !chunks[id].valid)
            return 0;
    }

    int* sequence = realloc(playlist->sequence,
            (size_t)(old_count + order_count) * sizeof(*sequence));
    if (!sequence)
        return -1;
    playlist->sequence = sequence;

    for (int i = 0; i < order_count; i++) {
        int selector = read_u16(order_offset + (off_t)i * 0x02, sf);
        playlist->sequence[old_count + i] = group_boundary + selector;
    }
    playlist->count = old_count + order_count;

    /* One output track has one loop point. Keep the first authored point when
     * a V1 MOV contributes more than one control playlist. */
    if (loop && !playlist->loop) {
        playlist->loop = 1;
        playlist->loop_start = old_count + loop_start;
    }
    return 1;
}

static bool init_bank(df_bank_t* bank, int containers, bool use_names) {
    bank->chunks = calloc((size_t)containers, sizeof(*bank->chunks));
    bank->audio_ids = malloc((size_t)containers * sizeof(*bank->audio_ids));
    if (use_names)
        bank->names = calloc((size_t)containers, sizeof(*bank->names));

    if (!bank->chunks || !bank->audio_ids ||
            (use_names && !bank->names))
        return false;

    bank->containers = containers;
    return true;
}

static void free_bank(df_bank_t* bank) {
    if (!bank)
        return;

    free(bank->chunks);
    free(bank->audio_ids);
    free(bank->playlist.sequence);
    free(bank->names);
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

/* Present every generation consistently: one authored background track first,
 * followed by each stored audio chunk in the same subsong number space.
 * stream_index 0 selects the background track. */
static VGMSTREAM* build_bank_subsong(STREAMFILE* sf, df_bank_t* bank) {
    VGMSTREAM* vgmstream = NULL;
    int subsongs = (bank->has_playlist ? 1 : 0) + bank->audio_count;
    int target = sf->stream_index;

    if (target == 0)
        target = 1;
    if (subsongs <= 0 || target < 1 || target > subsongs)
        return NULL;

    if (bank->has_playlist && target == 1) {
        df_playlist_t* playlist = &bank->playlist;
        int lo, hi, loop, loop_start;

        trim_playlist(sf, bank->chunks, playlist, &lo, &hi, &loop, &loop_start);
        vgmstream = build_segmented(sf, bank->chunks, playlist->sequence + lo,
                hi - lo + 1, loop, loop_start);
        if (!vgmstream)
            return NULL;

        set_stream_name(sf, vgmstream, bank->track_name, target);
    }
    else {
        int piece = target - (bank->has_playlist ? 1 : 0);
        int id = bank->audio_ids[piece - 1];

        vgmstream = allocate_vgmstream(1, 0);
        if (!vgmstream)
            return NULL;

        config_chunk(vgmstream, &bank->chunks[id]);
        set_stream_name(sf, vgmstream,
                bank->names ? bank->names[id].name : NULL, target);

        if (!vgmstream_open_stream(vgmstream, sf, bank->chunks[id].offset)) {
            close_vgmstream(vgmstream);
            return NULL;
        }
    }

    vgmstream->num_streams = subsongs;
    return vgmstream;
}

/* Revision-1 MOVs keep Group-A one-shots and Group-B background sources
 * immediately after each control. */
static VGMSTREAM* build_cf_df_v1_mov(STREAMFILE* sf, int containers,
        read_u16_t read_u16, read_u32_t read_u32) {
    VGMSTREAM* vgmstream = NULL;
    df_bank_t bank = {0};
    int control = 0;

    if (!init_bank(&bank, containers, false))
        goto fail;

    off_t file_size = get_streamfile_size(sf);
    while (control < containers) {
        off_t pos = read_u32(DF_HEADER_SIZE + (off_t)control * 0x04, sf);
        if (pos <= 0 || pos + 0x08 > file_size)
            goto fail;
        off_t p = pos + 0x08;

        df_mov_control_t cfg = {0};

        if (p + 0x8be + 0x04 > file_size)
            goto fail;
        if (read_u32(p + 0x00, sf) != DF_MOV_CONTROL_REVISION_1)
            goto fail;
        cfg.audio_a_count = read_u16(p + 0x1a, sf);
        cfg.audio_b_count = read_u16(p + 0x1c, sf);
        cfg.order_count = read_u16(p + 0x34, sf);
        cfg.order = p + 0x83e;
        uint32_t authored_loop = read_u32(p + 0x8be, sf);
        if (authored_loop < (uint32_t)cfg.order_count) {
            cfg.loop = 1;
            cfg.loop_start = authored_loop;
        }
        uint32_t authored_next = read_u32(p + 0x36, sf);
        if (authored_next > (uint32_t)containers)
            goto fail;
        cfg.next_control = authored_next ? (int)authored_next : containers;

        int audio_base = control + 1;
        int group_b_base = audio_base + cfg.audio_a_count;
        if (cfg.audio_a_count + cfg.audio_b_count > containers ||
                audio_base + cfg.audio_a_count + cfg.audio_b_count > containers)
            goto fail;
        if (cfg.next_control <= control || cfg.next_control > containers)
            goto fail;
        if (cfg.order_count > DF_MAX_CHUNKS)
            goto fail;
        if (cfg.order + (off_t)cfg.order_count * 0x02 > file_size)
            goto fail;

        for (int i = 0; i < cfg.audio_a_count + cfg.audio_b_count; i++) {
            int id = audio_base + i;
            if (!is_valid_chunk(sf, containers, id, read_u16, read_u32,
                    &bank.chunks[id]))
                goto fail;
            if (bank.audio_count >= containers)
                goto fail;
            bank.audio_ids[bank.audio_count++] = id;
        }

        if (cfg.audio_b_count > 0 && cfg.order_count > 0) {
            if (append_contiguous_playlist(sf, read_u16, bank.chunks, containers,
                    group_b_base - 1, cfg.audio_b_count, cfg.order, cfg.order_count,
                    cfg.loop, cfg.loop_start, &bank.playlist) != 1)
                goto fail;
            bank.has_playlist = true;
        }

        if (cfg.next_control == containers)
            break;
        control = cfg.next_control;
    }

    vgmstream = build_bank_subsong(sf, &bank);
    free_bank(&bank);
    return vgmstream;

fail:
    free_bank(&bank);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Early resources serialize the complete container in the platform's native byte order. */
static bool get_native_config(STREAMFILE* sf, read_u32_t read_u32, int* out_containers) {
    off_t file_size = get_streamfile_size(sf);
    if (file_size <= 0 || file_size > UINT32_MAX)
        return false;
    if (read_u32(0x00, sf) != (1u << 16) ||
            read_u32(0x04, sf) != (uint32_t)file_size)
        return false;

    uint32_t containers = read_u32(0x14, sf);
    if (containers <= 1 || containers > INT16_MAX)
        return false;
    if (DF_HEADER_SIZE + (off_t)containers * 0x04 > file_size)
        return false;

    *out_containers = containers;
    return true;
}

/* Proto resources may use either native byte order and have no family tag. */
static bool get_proto_config(STREAMFILE* sf, int* out_containers, bool* out_big_endian) {
    if (is_id32be(0x20, sf, "LPPA") && is_id32be(0x24, sf, "LPPA"))
        return false;

    int containers_le = 0;
    int containers_be = 0;
    bool valid_le = get_native_config(sf, read_u32le, &containers_le);
    bool valid_be = get_native_config(sf, read_u32be, &containers_be);
    if (valid_le == valid_be)
        return false;

    *out_containers = valid_be ? containers_be : containers_le;
    *out_big_endian = valid_be;
    return true;
}

/* Proto standalone music banks use the same Group-A/Group-B model as early
 * .snd, but place the compact control block at the start of container 0. */
static int get_proto_bank_playlist(STREAMFILE* sf, int containers,
        read_u16_t read_u16, read_u32_t read_u32, df_chunk_t* chunks,
        df_playlist_t* playlist) {
    off_t file_size = get_streamfile_size(sf);
    off_t control = read_u32(DF_HEADER_SIZE, sf);

    if (control <= 0 || control + 0x08 > file_size || read_u32(control, sf) != 0)
        return 0;

    uint32_t payload_size = read_u32(control + 0x04, sf);
    off_t p = control + 0x08;
    if (payload_size < 0x06 + DF_PROTO_PLAYLIST_MAX * 0x02 || payload_size > file_size - p)
        return 0;

    df_bank_control_t cfg = {
        .group_a_boundary = read_u16(p + 0x00, sf),
        .group_b_count = read_u16(p + 0x02, sf),
        .order_count = read_u16(p + 0x04, sf),
        .order = p + 0x06,
    };
    if (cfg.group_a_boundary <= 0 || cfg.group_b_count <= 0 ||
        cfg.order_count <= 0 || cfg.order_count > DF_PROTO_PLAYLIST_MAX ||
        cfg.group_a_boundary + cfg.group_b_count >= containers)
        return 0;

    for (int selector = 1; selector <= cfg.group_b_count; selector++) {
        int id = cfg.group_a_boundary + selector;
        if (!chunks[id].valid)
            return 0;
    }

    return append_contiguous_playlist(sf, read_u16, chunks, containers,
            cfg.group_a_boundary, cfg.group_b_count, cfg.order,
            cfg.order_count, 0, 0, playlist);
}

/* Proto containers intersperse sound with non-sound resources.
 * Discover only self-validating audio throughout the file. MOV resources may
 * additionally carry a complete Group-B playlist in control 0. Non-MOV files
 * are independently checked for the compact standalone music-bank form. */
static VGMSTREAM* build_cf_df_proto(STREAMFILE* sf, int containers,
        bool big_endian, bool parse_movie_playlist) {
    VGMSTREAM* vgmstream = NULL;
    df_bank_t bank = {0};
    read_u16_t read_u16 = get_read_u16(big_endian);
    read_u32_t read_u32 = get_read_u32(big_endian);
    int sample_rate = big_endian ? DF_PROTO_MACINTOSH_RATE : DF_PROTO_WINDOWS_RATE;

    if (!init_bank(&bank, containers, false))
        goto fail;

    for (int id = 1; id < containers; id++) {
        if (is_valid_proto_chunk(sf, containers, id, read_u16, read_u32,
                sample_rate, &bank.chunks[id])) {
            bank.audio_ids[bank.audio_count++] = id;
        }
    }
    if (bank.audio_count <= 0)
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

            if (payload_size <= file_size - p && payload_size >= 0x1c + 0x02) {
                df_bank_control_t cfg = {
                    .group_a_boundary = read_u16(p + 0x02, sf),
                    .group_b_count = read_u16(p + 0x04, sf),
                    .order_count = read_u16(p + 0x1c, sf),
                    .order = p + 0x822,
                };

                if (cfg.group_b_count > 0 && cfg.order_count > 0 &&
                    cfg.order_count <= DF_PROTO_PLAYLIST_MAX &&
                    cfg.group_a_boundary + cfg.group_b_count < containers &&
                    0x822 + (off_t)cfg.order_count * 0x02 <= payload_size) {
                    int result = append_contiguous_playlist(sf, read_u16,
                            bank.chunks, containers, cfg.group_a_boundary, cfg.group_b_count,
                            cfg.order, cfg.order_count,
                            1, 0, &bank.playlist);
                    if (result < 0)
                        goto fail;
                    if (result > 0)
                        bank.has_playlist = true;
                }
            }
        }
    }
    else {
        int playlist_result = get_proto_bank_playlist(sf, containers,
                read_u16, read_u32, bank.chunks, &bank.playlist);
        if (playlist_result < 0)
            goto fail;
        bank.has_playlist = playlist_result > 0;
    }

    vgmstream = build_bank_subsong(sf, &bank);
    free_bank(&bank);
    return vgmstream;

fail:
    free_bank(&bank);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Assembled track via segmented layout: play the given sequence of loop chunks once.
 * loop=1 marks the whole assembled track as an end-to-end loop (.snd/.sfx/.trk/.11k only; .mov excluded). */
static VGMSTREAM* build_segmented(STREAMFILE* sf, df_chunk_t* chunks, const int* seq, int count, int loop, int loop_start) {
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
    v->coding_type  = (loop[lo].codec_flag == DF_CODEC_V40) ? coding_CF_DF_ADPCM_V40 : coding_CF_DF_DPCM_V41;
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

static bool get_v1_snd_config(STREAMFILE* sf, int containers,
        read_u16_t read_u16, read_u32_t read_u32, df_v1_snd_config_t* cfg) {
    off_t file_size = get_streamfile_size(sf);
    off_t control = read_u32(DF_HEADER_SIZE, sf);

    if (control <= 0 || control + 0xc2 > file_size)
        return false;

    cfg->bank.group_a_boundary = read_u16(control + 0x20, sf);
    cfg->bank.group_b_count = read_u16(control + 0x22, sf);
    cfg->bank.order_count = read_u16(control + 0x24, sf);
    cfg->bank.order = control + 0x26;
    cfg->track_name = control + 0xa6;
    cfg->chunk_names = control + 0xc2;
    if (cfg->bank.group_a_boundary <= 0 || cfg->bank.group_b_count <= 0 ||
            cfg->bank.group_a_boundary + cfg->bank.group_b_count >= containers ||
            cfg->bank.order_count <= 0 || cfg->bank.order_count > DF_SND_ORDER_MAX)
        return false;
    if (cfg->bank.order + (off_t)cfg->bank.order_count * 0x02 > file_size)
        return false;

    return true;
}

/* DreamFactory V1 .snd "container-0" variant: the assembled theme's order list, track name and
 * per-chunk names live in container 0, unlike the container 1 found in trk/sfx/mov.
 * Builds the theme (subsong 1: order assembled in order, leading/trailing silence trimmed,
 * end-to-end loop) plus every chunk as a named subsong. */
static VGMSTREAM* build_cf_df_v1_snd(STREAMFILE* sf, int containers,
        read_u16_t read_u16, read_u32_t read_u32, const df_v1_snd_config_t* cfg) {
    VGMSTREAM* vgmstream = NULL;
    df_bank_t bank = {0};
    off_t file_size = get_streamfile_size(sf);

    if (!init_bank(&bank, containers, true))
        goto fail;

    for (int i = 0; i < containers; i++)
        is_valid_chunk(sf, containers, i, read_u16, read_u32,
                &bank.chunks[i]); /* fills chunks[i], sets .valid */

    /* order list -> seq (1-based selectors into the contiguous Group-B range) */
    if (append_contiguous_playlist(sf, read_u16, bank.chunks, containers,
            cfg->bank.group_a_boundary, cfg->bank.group_b_count,
            cfg->bank.order, cfg->bank.order_count,
            1, 0, &bank.playlist) != 1)
        goto fail;
    bank.has_playlist = true;

    /* per-chunk names: entry k (0-based) -> container k+1 */
    for (int id = 1; id < containers; id++) {
        off_t e = cfg->chunk_names + (off_t)(id - 1) * DF_SND_NAME_STRIDE;
        if (e + 1 > file_size)
            break;
        int ln = read_u8(e, sf);
        if (ln <= 0 || ln > DF_SHORT_NAME_MAX || e + 1 + ln > file_size)
            continue;
        for (int k = 0; k < ln; k++)
            bank.names[id].name[k] = read_u8(e + 1 + k, sf);
        bank.names[id].name[ln] = '\0';
    }

    /* track name */
    {
        int tl = read_u8(cfg->track_name, sf);
        if (tl > 0 && tl <= DF_MOV_NAME_MAX && cfg->track_name + 1 + tl <= file_size) {
            for (int k = 0; k < tl; k++)
                bank.track_name[k] = read_u8(cfg->track_name + 1 + k, sf);
            bank.track_name[tl] = '\0';
        }
    }

    /* individual list: every valid audio chunk, kept (not folded) so each stays its own named subsong */
    for (int i = 0; i < containers; i++)
        if (bank.chunks[i].valid)
            bank.audio_ids[bank.audio_count++] = i;

    vgmstream = build_bank_subsong(sf, &bank);
    free_bank(&bank);
    return vgmstream;

fail:
    free_bank(&bank);
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* build_cf_df_v1(STREAMFILE* sf, int containers, bool is_mov,
        bool big_endian, const df_v1_snd_config_t* snd_config) {
    read_u16_t read_u16 = get_read_u16(big_endian);
    read_u32_t read_u32 = get_read_u32(big_endian);

    if (is_mov)
        return build_cf_df_v1_mov(sf, containers, read_u16, read_u32);

    return build_cf_df_v1_snd(sf, containers, read_u16, read_u32, snd_config);
}

static VGMSTREAM* build_cf_df_v4(STREAMFILE* sf, int containers, bool is_mov) {
    VGMSTREAM* vgmstream = NULL;
    df_bank_t bank = {0};
    df_chunk_t* loop = NULL;
    int16_t* order = NULL;
    uint8_t* in_loop = NULL;

    int loop_track = check_extensions(sf, "sfx,snd,trk,11k");
    int target = sf->stream_index;
    if (target == 0)
        target = 1;

    // definitions before 'goto fail'
    int loop_count, order_count, loop_block_id;
    off_t first_entry, c1, c0;
    int disk_stream;

    if (!init_bank(&bank, containers, true))
        goto fail;

    /* --- loop block (container 1): chunkOrder + loop chunk list --- */
    loop_count = 0;
    order_count = 0;
    first_entry = 0;
    loop_block_id = check_extensions(sf, "snd") ? get_snd_loop_block_id(sf, containers) : 1;
    c1 = loop_block_id > 0 ? read_u32le(DF_HEADER_SIZE + (off_t)loop_block_id * 0x04, sf) : 0;
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
                    read_name(sf, e, DF_SHORT_NAME_MAX, &bank.names[id]);
                if (!is_valid_chunk(sf, containers, id, read_u16le, read_u32le,
                        &loop[i])) {
                    all_valid = 0;
                }
                else {
                    bank.chunks[id] = loop[i];
                }
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
                    bank.track_name[k] = read_u8(p0 + 0x25 + k, sf);
                bank.track_name[tl] = '\0';
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
                            read_name(sf, e, name_max, &bank.names[id]);
                        e += stride;
                    }
                }
            }
        }
    }

    for (int i = 0; i < containers; i++) {
        if (in_loop && in_loop[i])
            continue;
        if (is_valid_chunk(sf, containers, i, read_u16le, read_u32le,
                &bank.chunks[i]))
            bank.audio_ids[bank.audio_count++] = i;
    }

    if (loop_count > 0) {
        bool blocked_stream = disk_stream && loop[0].codec_flag == DF_CODEC_V41;
        int use_order = order_count > 0;
        int sequence_count;

        for (int i = 0; use_order && i < order_count; i++) {
            int index = order[i] - 1;
            if (index < 0 || index >= loop_count)
                use_order = false;
        }

        sequence_count = use_order ? order_count : loop_count;
        if (!blocked_stream) {
            bank.playlist.sequence = malloc((size_t)sequence_count * sizeof(*bank.playlist.sequence));
            if (!bank.playlist.sequence)
                goto fail;

            for (int i = 0; i < sequence_count; i++) {
                int index = use_order ? order[i] - 1 : i;
                bank.playlist.sequence[i] = loop[index].container_id;
            }
            bank.playlist.count = sequence_count;
            bank.playlist.loop = loop_track;
        }
        bank.has_playlist = true;

        if (blocked_stream && target == 1) {
            vgmstream = build_blocked(sf, loop, loop_count, first_entry);
            if (!vgmstream)
                goto fail;
            set_stream_name(sf, vgmstream,
                    !is_mov ? bank.track_name : NULL, target);
            vgmstream->num_streams = 1 + bank.audio_count;
        }
        else {
            vgmstream = build_bank_subsong(sf, &bank);
        }
    }
    else {
        vgmstream = build_bank_subsong(sf, &bank);
    }

    free_bank(&bank);
    free(loop);
    free(order);
    free(in_loop);
    return vgmstream;

fail:
    free_bank(&bank);
    free(loop);
    free(order);
    free(in_loop);
    close_vgmstream(vgmstream);
    return NULL;
}

VGMSTREAM* init_vgmstream_cf_df(STREAMFILE* sf) {
    df_version_t version = DF_VERSION_NONE;
    df_v1_snd_config_t snd_config = {0};
    int containers = 0;
    bool is_mov;
    bool proto_big_endian = false;
    bool v1_big_endian = false;
    bool is_extensionless;

    if (!check_extensions(sf, "snd,sfx,trk,11k,mov,move,"))
        return NULL;

    is_mov = check_extensions(sf, "mov,move");
    is_extensionless = check_extensions(sf, ""); //Proto has banked music!

    /* Macintosh Dust V1 uses native big-endian fields and family tags in
     * place of the later LPPALPPA marker. */
    bool is_mac_v1_mov = is_mov && is_id64be(0x20, sf, "MOVEDFME");
    bool is_mac_v1_snd = check_extensions(sf, "snd") &&
            is_id64be(0x20, sf, "SONGDFST");
    if (is_mac_v1_mov || is_mac_v1_snd) {
        if (!get_native_config(sf, read_u32be, &containers))
            return NULL;

        if (is_mac_v1_mov) {
            if (!has_v1_mov_control(sf, read_u32be))
                return NULL;
        }
        else if (!get_v1_snd_config(sf, containers,
                read_u16be, read_u32be, &snd_config)) {
            return NULL;
        }

        version = DF_VERSION_1;
        v1_big_endian = true;
    }

    /* Proto has no LPPALPPA marker and may be native little- or big-endian.
     * Extensionless files are admitted only after this complete header check. */
    if (version == DF_VERSION_NONE && (is_mov || is_extensionless)) {
        if (get_proto_config(sf, &containers, &proto_big_endian)) {
            version = DF_VERSION_PROTO;
        }
        else if (is_extensionless) {
            return NULL;
        }
    }

    if (version == DF_VERSION_NONE) {
        if (read_u32le(0x04, sf) != get_streamfile_size(sf))
            return NULL;

        containers = read_u32le(0x14, sf);
        if (containers <= 0 || containers > INT16_MAX)
            return NULL;
        if (DF_HEADER_SIZE + (off_t)containers * 0x04 > get_streamfile_size(sf))
            return NULL;

        if (is_mov && has_v1_mov_control(sf, read_u32le)) {
            version = DF_VERSION_1;
        }
        else {
            if (!(is_id32be(0x20, sf, "LPPA") && is_id32be(0x24, sf, "LPPA")))
                return NULL;

            /* V1 .snd and V4 share the later envelope. Prefer the structural
             * V1 container-0 playlist, then use the V4 loop-block model. */
            if (check_extensions(sf, "snd") && get_v1_snd_config(sf, containers,
                    read_u16le, read_u32le, &snd_config))
                version = DF_VERSION_1;
            else
                version = DF_VERSION_4;
        }
    }

    switch (version) {
        case DF_VERSION_PROTO:
            return build_cf_df_proto(sf, containers, proto_big_endian, is_mov);
        case DF_VERSION_1:
            return build_cf_df_v1(sf, containers, is_mov, v1_big_endian, &snd_config);
        case DF_VERSION_4:
            return build_cf_df_v4(sf, containers, is_mov);
        default:
            return NULL;
    }
}
