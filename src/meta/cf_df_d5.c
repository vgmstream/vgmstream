#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/layout_utils.h"

#define DF_HEADER_SIZE  0x400
#define DF_MAX_CHUNKS   0x4000  /* sanity cap for table sizes */

/*
 * CF_DF_d5 (v5) - CyberFlix DreamFactory Engine, version 5 (.move / .trak).
 *
 * v5 keeps the v4 (cf_df) container envelope (file size @0x04, container count @0x14, u32 offset table
 * @0x400) but replaces the magic and audio model:
 * - Header magic @0x20 is EA-style 4CCs 'MOVE''D5ME' (or 'TRAK''D5ST'), stored LE.
 * - Containers are self-describing via a 4CC at payload +0x0C (stored LE): MFRM/STEP (video),
 *   SOUN (audio), MHED, MTHM (theme/playlist), MSND (sound director), MTRG (trigger). A multi-scene
 *   movie repeats MHED/MTHM/MSND/MTRG per scene (each scene's containers numbered from a base).
 * - SOUN payload base head = container + 0x08. Codec is fixed per stream and selected by head+0x1a
 *   (==1 -> v4.0 ADPCM), else head+0x18 (==0 -> v4.1 DPCM), else IMA. Sample rate at head+0x1c (native
 *   11025/22050/44100). Block count at head+0x28; block-offset table at head+0x2c (input offsets
 *   relative to head). Codec state resets every block;
 *
 * MTHM is the v5 analog of v4's container-1 loop block: a fixed-size order region (order_count u16
 * @theme_head+0x1c, order list u16[] @theme_head+0x1e, 1-based indices) followed by a segment array
 * (segment_count u32 @theme_head+0x222, array @theme_head+0x226 stride 0x22). Each segment descriptor
 * holds the absolute SOUN container id @+0x0c and a Pascal-string name @+0x12.
 *
 * Two movie shapes (mirroring v4):
 * - Disk-streamed (segment_count > order_count, e.g. segments named "disk 0"/"disk 8192"/...): the
 *   whole segment array is one continuous audio stream, stitched as subsong 1; the individual
 *   fragments are not listed separately.
 * - Named one-shots (order_count >= segment_count): subsong 1 follows the order list (with leading/
 *   trailing silence trimmed), and every SOUN is also listed individually (silent ones included).
 *
 * Titles: Redjack Revenge of the Brethern, Disney's Villain's Revenge, Disney's The D show
 */

#define DF_D5_THEME_ORDER_COUNT  0x1c
#define DF_D5_THEME_ORDER        0x1e
#define DF_D5_THEME_SEG_COUNT    0x222
#define DF_D5_THEME_SEG          0x226
#define DF_D5_THEME_SEG_STRIDE   0x22
#define DF_D5_THEME_SEG_SOUN     0x0c
#define DF_D5_THEME_SEG_NAME     0x12   /* Pascal string */

/* .trak (TRAK/D5ST) variant. Same container envelope and the same order list (order_count @0x1c,
 * order list @0x1e) and seg_count (@0x222) as .move, but the theme is STHM (not MTHM), the sound
 * director is SSND (not MSND), and both sub-records have their own layout. SSND ids are ABSOLUTE
 * container indices (no scene-relative MHED math). */
#define DF_D5_TRAK_SEG          0x22a  /* theme (STHM) segment array, rel theme_head */
#define DF_D5_TRAK_SEG_STRIDE   0x1a
#define DF_D5_TRAK_SEG_SOUN     0x00   /* u32 absolute SOUN container id */
#define DF_D5_TRAK_SEG_NAME     0x06   /* Pascal string (len @ +0x06, chars @ +0x07) */
#define DF_D5_TRAK_DIR_COUNT    0x1c   /* SSND entry count, rel dir head */
#define DF_D5_TRAK_DIR_ENTRY    0x20   /* first SSND entry, rel dir head */
#define DF_D5_TRAK_DIR_STRIDE   0x1a
#define DF_D5_TRAK_DIR_SOUN     0x04   /* u32 absolute SOUN container id */
#define DF_D5_TRAK_DIR_NAME     0x0a   /* Pascal string (len @ +0x0a, chars @ +0x0b) */

#define DF_D5_SOUN_V41_SELECTOR  0x18   /* u16; ==0 -> v4.1 DPCM */
#define DF_D5_SOUN_V40_SELECTOR  0x1a   /* u16; ==1 -> v4.0 ADPCM */
#define DF_D5_SOUN_RATE          0x1c   /* u32; native 11025/22050/44100 */
#define DF_D5_SOUN_BLOCK_COUNT   0x28   /* u32; mixer loop bound */
#define DF_D5_SOUN_BLOCK_TABLE   0x2c   /* u32[]; block offsets relative to head */

/* Parsed SOUN header */
typedef struct {
    off_t    cont_offset;     /* container start in file */
    off_t    head;            /* cont_offset + 0x08, SOUN payload base */
    uint32_t cont_size;       /* container size @ cont_offset+0x04 */
    int      v41_selector;    /* head+0x18 */
    int      v40_selector;    /* head+0x1a */
    int      rate;            /* head+0x1c */
    int      block_count;     /* head+0x28 */
    off_t    block_table;     /* head+0x2c */
    int      coding;          /* derived from the selectors */
} df_d5_soun_t;

/* Result of cf_df_d5_read_theme_move: the segment array and the playback sequence (both SOUN ids). */
typedef struct {
    int* seg_souns; int seg_count;   /* every segment's SOUN id, in array order */
    int* seq;       int seq_count;   /* playback sequence of SOUN ids */
    bool disk;                       /* disk-streamed (seg_count > order_count) */
} df_d5_theme_move_t;

/* Common SOUN parse */
static bool cf_df_d5_parse_soun(STREAMFILE* sf, int soun_id, df_d5_soun_t* s) {
    s->cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)soun_id * 0x04, sf);
    if (s->cont_offset <= 0 || s->cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
        return false;
    if (!is_id32le(s->cont_offset + 0x0C, sf, "SOUN"))
        return false;

    s->head         = s->cont_offset + 0x08;
    s->cont_size    = read_u32le(s->cont_offset + 0x04, sf);
    s->v41_selector = read_u16le(s->head + DF_D5_SOUN_V41_SELECTOR, sf);
    s->v40_selector = read_u16le(s->head + DF_D5_SOUN_V40_SELECTOR, sf);
    s->rate         = read_u32le(s->head + DF_D5_SOUN_RATE, sf);
    s->block_count  = read_u32le(s->head + DF_D5_SOUN_BLOCK_COUNT, sf);
    s->block_table  = s->head + DF_D5_SOUN_BLOCK_TABLE;
    if (s->block_count <= 0 || s->block_count > DF_MAX_CHUNKS)
        return false;

    s->coding = (s->v40_selector == 1) ? coding_CF_DF_ADPCM_v5
              : (s->v41_selector == 0) ? coding_CF_DF_DPCM_V41
                                       : coding_CF_DF_IMA_v5;
    return true;
}

/* Input range of block k, relative to head: [*start, *end). false if it overruns the container.
 * The block-offset table has block_count+1 entries: table[k+1] (incl. the terminal entry for the last
 * block) is the true end. container_size includes alignment padding, which would otherwise decode as
 * trailing garbage (e.g. a padding 0xff read as a v4.0 Mode III run). */
static bool cf_df_d5_block_range(STREAMFILE* sf, const df_d5_soun_t* s, int k,
                                 uint32_t* start, uint32_t* end) {
    *start = read_u32le(s->block_table + (off_t)k * 0x04, sf);
    *end   = read_u32le(s->block_table + (off_t)(k + 1) * 0x04, sf);
    return (*end >= *start && *end <= s->cont_size);
}

/* Resolve a SOUN's display name. Priority: (1) the Pascal-string name on a theme segment descriptor
 * that references it; else (2) the MSND sound director. MSND ids are scene-relative: a multi-scene
 * movie repeats MHED(+0)/MTHM(+1)/MSND(+2)/MTRG(+3) per scene, so an MSND at container index m has
 * scene base m-2 and its entries map to absolute SOUN base+rel. Leaves dst empty if unnamed. */
static void cf_df_d5_lookup_name_move(STREAMFILE* sf, int containers, int soun_id, char* dst, int dst_size) {
    dst[0] = '\0';

    /* (1) theme segment-descriptor name */
    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;
        if (!is_id32le(cont_offset + 0x0C, sf, "MTHM"))
            continue;

        off_t theme_head = cont_offset + 0x08;
        uint32_t seg_count = read_u32le(theme_head + DF_D5_THEME_SEG_COUNT, sf);
        if (seg_count == 0 || seg_count > DF_MAX_CHUNKS)
            continue;
        for (uint32_t s = 0; s < seg_count; s++) {
            off_t seg_desc = theme_head + DF_D5_THEME_SEG + (off_t)s * DF_D5_THEME_SEG_STRIDE;
            if ((int)read_u32le(seg_desc + DF_D5_THEME_SEG_SOUN, sf) != soun_id)
                continue;
            int len = read_u8(seg_desc + DF_D5_THEME_SEG_NAME, sf);
            if (len <= 0)
                break;
            if (len > dst_size - 1)
                len = dst_size - 1;
            if (seg_desc + DF_D5_THEME_SEG_NAME + 1 + len > (off_t)get_streamfile_size(sf))
                break;
            for (int c = 0; c < len; c++)
                dst[c] = read_u8(seg_desc + DF_D5_THEME_SEG_NAME + 1 + c, sf);
            dst[len] = '\0';
            return;
        }
    }

    /* (2) MSND sound director (scene-relative) */
    int current_scene_base = 0;
    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;

        if (is_id32le(cont_offset + 0x0C, sf, "MHED")) {
            current_scene_base = i;
        }
        if (!is_id32le(cont_offset + 0x0C, sf, "MSND"))
            continue;

        int scene_base = current_scene_base;

        off_t head = cont_offset + 0x08; /* MSND payload */
        int entry_count = read_u32le(head + 0x18, sf); /* MSND entry count */
        if (entry_count <= 0 || entry_count > DF_MAX_CHUNKS)
            continue;

        for (int k = 0; k < entry_count; k++) {
            off_t entry = head + 0x28 + (off_t)k * 0x30; /* MSND entries, stride 0x30 */
            if (entry + 0x09 > (off_t)get_streamfile_size(sf))
                break;
            if (scene_base + read_u16le(entry + 0x02, sf) != soun_id) /* rel SOUN id */
                continue;

            int len = read_u8(entry + 0x08, sf); /* name len, chars at +0x09 */
            if (len > dst_size - 1)
                len = dst_size - 1;
            if (entry + 0x09 + len > (off_t)get_streamfile_size(sf))
                return;
            for (int c = 0; c < len; c++)
                dst[c] = read_u8(entry + 0x09 + c, sf);
            dst[len] = '\0';
            return;
        }
    }
}

/* .trak name resolution. Priority: (1) the SSND sound director (absolute SOUN ids string names;
 * else (2) the STHM theme segment label. Pascal length is the exact char count. Leaves dst empty if unnamed. */
static void cf_df_d5_lookup_name_trak(STREAMFILE* sf, int containers, int soun_id, char* dst, int dst_size) {
    dst[0] = '\0';

    /* (1) SSND sound director */
    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;
        if (!is_id32le(cont_offset + 0x0C, sf, "SSND"))
            continue;

        off_t head = cont_offset + 0x08;
        int entry_count = read_u32le(head + DF_D5_TRAK_DIR_COUNT, sf);
        if (entry_count <= 0 || entry_count > DF_MAX_CHUNKS)
            continue;

        for (int k = 0; k < entry_count; k++) {
            off_t entry = head + DF_D5_TRAK_DIR_ENTRY + (off_t)k * DF_D5_TRAK_DIR_STRIDE;
            if (entry + DF_D5_TRAK_DIR_NAME + 1 > (off_t)get_streamfile_size(sf))
                break;
            if ((int)read_u32le(entry + DF_D5_TRAK_DIR_SOUN, sf) != soun_id)
                continue;

            int len = read_u8(entry + DF_D5_TRAK_DIR_NAME, sf);
            if (len <= 0)
                break;
            if (len > dst_size - 1)
                len = dst_size - 1;
            if (entry + DF_D5_TRAK_DIR_NAME + 1 + len > (off_t)get_streamfile_size(sf))
                break;
            for (int c = 0; c < len; c++)
                dst[c] = read_u8(entry + DF_D5_TRAK_DIR_NAME + 1 + c, sf);
            dst[len] = '\0';
            return;
        }
    }

    /* (2) STHM theme segment label */
    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;
        if (!is_id32le(cont_offset + 0x0C, sf, "STHM"))
            continue;

        off_t theme_head = cont_offset + 0x08;
        uint32_t seg_count = read_u32le(theme_head + DF_D5_THEME_SEG_COUNT, sf);
        if (seg_count == 0 || seg_count > DF_MAX_CHUNKS)
            continue;
        for (uint32_t s = 0; s < seg_count; s++) {
            off_t seg_desc = theme_head + DF_D5_TRAK_SEG + (off_t)s * DF_D5_TRAK_SEG_STRIDE;
            if ((int)read_u32le(seg_desc + DF_D5_TRAK_SEG_SOUN, sf) != soun_id)
                continue;
            int len = read_u8(seg_desc + DF_D5_TRAK_SEG_NAME, sf);
            if (len <= 0)
                break;
            if (len > dst_size - 1)
                len = dst_size - 1;
            if (seg_desc + DF_D5_TRAK_SEG_NAME + 1 + len > (off_t)get_streamfile_size(sf))
                break;
            for (int c = 0; c < len; c++)
                dst[c] = read_u8(seg_desc + DF_D5_TRAK_SEG_NAME + 1 + c, sf);
            dst[len] = '\0';
            return;
        }
    }
}

static bool cf_df_d5_soun_is_silent(STREAMFILE* sf, int soun_id) {
    df_d5_soun_t s;
    if (!cf_df_d5_parse_soun(sf, soun_id, &s))
        return false;

    uint8_t buf[0x1000];
    for (int k = 0; k < s.block_count; k++) {
        uint32_t block_start, block_end;
        if (!cf_df_d5_block_range(sf, &s, k, &block_start, &block_end))
            return false;

        off_t p = s.head + block_start;
        int32_t left = (int32_t)(block_end - block_start);
        while (left > 0) {
            int to_read = left > (int)sizeof(buf) ? (int)sizeof(buf) : left;
            int got = read_streamfile(buf, p, to_read, sf);
            if (got <= 0)
                return false;
            for (int i = 0; i < got; i++) {
                uint8_t b = buf[i];
                if (s.coding == coding_CF_DF_DPCM_V41) {
                    if (b != 0x00 && b != 0x80)
                        return false;
                } else if (s.coding == coding_CF_DF_IMA_v5) {
                    if (b != 0x00)
                        return false;
                } else { /* v4.0 ADPCM */
                    if (b != 0x40 && b < 0xC0)
                        return false;
                }
            }
            p    += got;
            left -= got;
        }
    }
    return true;
}

static VGMSTREAM* build_d5_soun(STREAMFILE* sf, int soun_id) {
    VGMSTREAM* vgmstream = NULL;

    df_d5_soun_t s;
    if (!cf_df_d5_parse_soun(sf, soun_id, &s))
        return NULL;
    if (s.rate != 11025 && s.rate != 22050 && s.rate != 44100)
        return NULL;
    if (s.block_table + (off_t)(s.block_count + 1) * 0x04 > s.head + s.cont_size) /* +1: terminal entry */
        return NULL;

    int32_t num_samples = 0;
    for (int k = 0; k < s.block_count; k++) {
        uint32_t block_start, block_end;
        if (!cf_df_d5_block_range(sf, &s, k, &block_start, &block_end))
            return NULL;
        int block_size = (int)(block_end - block_start);
        off_t block_data = s.head + block_start;

        if (s.coding == coding_CF_DF_IMA_v5) {
            if (block_size < 3) continue;
            int step_index = read_u8(block_data + 0x02, sf);
            num_samples += (step_index > 0x58) ? 0 : (1 + 2 * (block_size - 3));
        }
        else if (s.coding == coding_CF_DF_ADPCM_v5) {
            if (block_size < 1) continue;
            num_samples += cf_df_v5_get_samples(sf, block_data, block_size);
        }
        else { /* v4.1 */
            num_samples += block_size;
        }
    }
    if (num_samples <= 0)
        return NULL;

    vgmstream = allocate_vgmstream(1, 0);
    if (!vgmstream) return NULL;

    vgmstream->meta_type   = meta_CF_DF_D5;
    vgmstream->sample_rate = s.rate;
    vgmstream->num_samples = num_samples;
    vgmstream->stream_size = s.cont_size;
    vgmstream->coding_type = s.coding;
    vgmstream->layout_type = layout_blocked_cf_df_v5;

    if (!vgmstream_open_stream(vgmstream, sf, s.block_table)) {
        close_vgmstream(vgmstream);
        return NULL;
    }
    return vgmstream;
}

// Find the first MTHM container that carries a usable theme (segment_count > 0).
static int cf_df_d5_find_theme(STREAMFILE* sf, int containers) {
    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;
        if (!is_id32le(cont_offset + 0x0C, sf, "MTHM"))
            continue;
        off_t theme_head = cont_offset + 0x08;
        uint32_t seg_count = read_u32le(theme_head + DF_D5_THEME_SEG_COUNT, sf);
        if (seg_count > 0 && seg_count <= DF_MAX_CHUNKS)
            return i;
    }
    return -1;
}

/* Read a theme into: seg_souns[] (every segment's SOUN id, in array order) and seq[] (the playback
 * sequence of SOUN ids). disk_stream movies (segment_count > order_count) play the whole segment
 * array; otherwise the sequence follows the 1-based order list. Returns false (no track) if the
 * theme is unparseable or any reference doesn't resolve to a real SOUN -- so the file degrades to a
 * plain SOUN list instead of failing or assembling garbage. */
static bool cf_df_d5_read_theme_move(STREAMFILE* sf, int containers, int theme_id, df_d5_theme_move_t* out) {
    out->seg_souns = NULL; out->seg_count = 0;
    out->seq = NULL;       out->seq_count = 0;
    out->disk = false;

    off_t theme_head = read_u32le(DF_HEADER_SIZE + (off_t)theme_id * 0x04, sf) + 0x08;
    int order_count = read_u16le(theme_head + DF_D5_THEME_ORDER_COUNT, sf);
    uint32_t seg_count = read_u32le(theme_head + DF_D5_THEME_SEG_COUNT, sf);
    if (seg_count == 0 || seg_count > DF_MAX_CHUNKS)
        return false;
    if (order_count < 0 || order_count > DF_MAX_CHUNKS)
        return false;

    int* segs = malloc((size_t)seg_count * sizeof(int));
    if (!segs) return false;

    for (uint32_t s = 0; s < seg_count; s++) {
        off_t seg_desc = theme_head + DF_D5_THEME_SEG + (off_t)s * DF_D5_THEME_SEG_STRIDE;
        int soun_id = read_u32le(seg_desc + DF_D5_THEME_SEG_SOUN, sf);
        off_t soun_offset = (soun_id >= 0 && soun_id < containers)
                   ? read_u32le(DF_HEADER_SIZE + (off_t)soun_id * 0x04, sf) : 0;
        if (soun_id < 0 || soun_id >= containers || soun_offset <= 0 ||
                soun_offset + 0x30 > (off_t)get_streamfile_size(sf) || !is_id32le(soun_offset + 0x0C, sf, "SOUN")) {
            free(segs);
            return false;
        }
        segs[s] = soun_id;
    }

    bool disk = (seg_count > (uint32_t)order_count);
    int* seq = NULL;
    int seq_count = 0;

    if (disk) {
        /* disk-streamed: the whole segment array, in order, is one continuous stream */
        seq = malloc((size_t)seg_count * sizeof(int));
        if (!seq) { free(segs); return false; }
        for (uint32_t s = 0; s < seg_count; s++)
            seq[s] = segs[s];
        seq_count = seg_count;
    } else {
        /* named one-shots: follow the 1-based order list */
        if (order_count <= 0) { free(segs); return false; }
        seq = malloc((size_t)order_count * sizeof(int));
        if (!seq) { free(segs); return false; }
        for (int i = 0; i < order_count; i++) {
            int order_entry = read_u16le(theme_head + DF_D5_THEME_ORDER + (off_t)i * 0x02, sf); /* 1-based */
            if (order_entry < 1 || (uint32_t)order_entry > seg_count) { free(seq); free(segs); return false; }
            seq[i] = segs[order_entry - 1];
        }
        seq_count = order_count;
    }

    out->seg_souns = segs; out->seg_count = (int)seg_count;
    out->seq = seq;        out->seq_count = seq_count;
    out->disk = disk;
    return true;
}

/* Assemble the background track: play the given SOUN sequence once via the segmented layout, like
 * the v4 path's build_segmented. Each step gets its own decoder (so a repeated SOUN is a distinct
* segment, never a double free). loop=1 marks the whole assembled track as an end-to-end loop. */
static VGMSTREAM* build_d5_track(STREAMFILE* sf, int* seq, int count, int loop) {
    VGMSTREAM* v = NULL;
    segmented_layout_data* data = init_layout_segmented(count);
    if (!data)
        goto fail;

    for (int i = 0; i < count; i++) {
        VGMSTREAM* seg = build_d5_soun(sf, seq[i]);
        if (!seg)
            goto fail;
        data->segments[i] = seg;
    }

    if (!setup_layout_segmented(data))
        goto fail;

    v = allocate_segmented_vgmstream(data, loop, 0, count - 1);
    if (!v)
        goto fail;
    return v;

fail:
    free_layout_segmented(data);
    close_vgmstream(v);
    return NULL;
}

/* Build the .trak playback sequence: the 1-based order list mapped through the STHM segment array to
 * absolute SOUN ids. Returns NULL when there is no stitched track -- no STHM,
 * empty order list, or any reference that doesn't resolve to a real SOUN.
 * Unlike .move there is no disk/one-shot split: the stitch is always exactly the order list. */
static int* cf_df_d5_read_theme_trak(STREAMFILE* sf, int containers, int* out_count) {
    *out_count = 0;

    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;
        if (!is_id32le(cont_offset + 0x0C, sf, "STHM"))
            continue;

        off_t theme_head = cont_offset + 0x08;
        int order_count = read_u16le(theme_head + DF_D5_THEME_ORDER_COUNT, sf);
        uint32_t seg_count = read_u32le(theme_head + DF_D5_THEME_SEG_COUNT, sf);
        if (order_count <= 0 || order_count > DF_MAX_CHUNKS)
            return NULL; /* no playable order, lists SOUNs individually */
        if (seg_count == 0 || seg_count > DF_MAX_CHUNKS)
            return NULL;

        int* seq = malloc((size_t)order_count * sizeof(int));
        if (!seq)
            return NULL;

        for (int o = 0; o < order_count; o++) {
            int order_entry = read_u16le(theme_head + DF_D5_THEME_ORDER + (off_t)o * 0x02, sf); /* 1-based */
            if (order_entry < 1 || (uint32_t)order_entry > seg_count) {
                free(seq);
                return NULL;
            }
            off_t seg_desc = theme_head + DF_D5_TRAK_SEG + (off_t)(order_entry - 1) * DF_D5_TRAK_SEG_STRIDE;
            int soun_id = read_u32le(seg_desc + DF_D5_TRAK_SEG_SOUN, sf);
            off_t soun_offset = (soun_id >= 0 && soun_id < containers)
                       ? read_u32le(DF_HEADER_SIZE + (off_t)soun_id * 0x04, sf) : 0;
            if (soun_id < 0 || soun_id >= containers || soun_offset <= 0 ||
                    soun_offset + 0x30 > (off_t)get_streamfile_size(sf) || !is_id32le(soun_offset + 0x0C, sf, "SOUN")) {
                free(seq);
                return NULL;
            }
            seq[o] = soun_id;
        }

        *out_count = order_count;
        return seq;
    }

    return NULL;
}

/* .trak assembly, kept entirely separate from the .move logic below. A multi-segment order produces
 * the assembled track as subsong 1, and then EVERY SOUN is listed individually -- including the
 * stitched chunks, so named loops stay reachable as their own. Files with no order list
 * or a single-segment order get no track and list every SOUN.
 * No silence trimming (the .trak order lists are continuous). */
static VGMSTREAM* cf_df_d5_build_trak(STREAMFILE* sf, int containers, int* soun_ids, int soun_count) {
    VGMSTREAM* vgmstream = NULL;
    int* seq = NULL;
    int* listed = NULL;
    int seq_count = 0;
    int listed_count = 0;
    int subsongs = 0;
    int target;

    seq = cf_df_d5_read_theme_trak(sf, containers, &seq_count);
    /* A single-segment order is not a real stitch -- treat that lone SOUN as a normal individual
     * subsong so its name survives. Folding a 1-segment "stitch" into a filename-named track would
     * drop the only name it has. Only > 1 segments form a track. */
    bool has_track = (seq != NULL && seq_count > 1);

    /* individual list: every SOUN, including the stitched chunks */
    listed = malloc((size_t)soun_count * sizeof(int));
    if (!listed)
        goto fail;
    for (int i = 0; i < soun_count; i++)
        listed[listed_count++] = soun_ids[i];

    subsongs = (has_track ? 1 : 0) + listed_count;
    target = sf->stream_index;

    if (target == 0)
        target = 1;
    if (target < 0 || target > subsongs)
        goto fail;

    if (has_track && target == 1) {
        /* subsong 1: the assembled track. Trim leading/trailing intentional silence
         * but keep interspersed silence -- same policy as the .move named-one-shot path,
         * The trimmed segments still appear as individual subsongs. */
        int trim_lo = 0, trim_hi = seq_count - 1;
        while (trim_lo <= trim_hi && cf_df_d5_soun_is_silent(sf, seq[trim_lo]))
            trim_lo++;
        while (trim_hi >= trim_lo && cf_df_d5_soun_is_silent(sf, seq[trim_hi]))
            trim_hi--;
        if (trim_lo > trim_hi) { trim_lo = 0; trim_hi = seq_count - 1; } /* all silent: keep so it isn't empty */

        vgmstream = build_d5_track(sf, seq + trim_lo, trim_hi - trim_lo + 1, 1); /* .trak: loop end-to-end */
        if (!vgmstream)
            goto fail;

        char basename[STREAM_NAME_SIZE];
        get_streamfile_filename(sf, basename, sizeof(basename));
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", basename);
    }
    else {
        /* individual SOUN streams */
        int piece = target - (has_track ? 1 : 0); /* 1-based index into listed[] */
        int soun_id = listed[piece - 1];

        vgmstream = build_d5_soun(sf, soun_id);
        if (!vgmstream)
            goto fail;

        char name[STREAM_NAME_SIZE], basename[STREAM_NAME_SIZE];
        cf_df_d5_lookup_name_trak(sf, containers, soun_id, name, sizeof(name));
        if (name[0]) {
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", name);
        } else {
            get_streamfile_filename(sf, basename, sizeof(basename));
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%.*s#%d", STREAM_NAME_SIZE - (11 + 1 + 1), basename, piece);
        }
    }

    vgmstream->num_streams = subsongs;

    free(seq);
    free(listed);
    return vgmstream;

fail:
    free(seq);
    free(listed);
    close_vgmstream(vgmstream);
    return NULL;
}

VGMSTREAM* init_vgmstream_cf_df_d5(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int* soun_ids = NULL;
    int* listed = NULL;
    df_d5_theme_move_t theme = {0};
    int containers;
    int soun_count = 0;
    int theme_id;
    bool has_track;
    int listed_count = 0;
    int subsongs;
    int target;

    /* checks */
    if (!( (is_id32le(0x20, sf, "MOVE") && is_id32le(0x24, sf, "D5ME")) ||
           (is_id32le(0x20, sf, "TRAK") && is_id32le(0x24, sf, "D5ST")) ))
        return NULL;
    if (read_u32le(0x04, sf) != (off_t)get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "move,trak"))
        return NULL;

    containers = read_u32le(0x14, sf);
    if (containers <= 0 || containers > INT16_MAX)
        return NULL;

    /* collect SOUN containers */
    soun_ids = malloc(containers * sizeof(int));
    if (!soun_ids)
        goto fail;

    for (int i = 0; i < containers; i++) {
        off_t cont_offset = read_u32le(DF_HEADER_SIZE + i * 0x04, sf);
        if (cont_offset <= 0 || cont_offset + 0x30 > (off_t)get_streamfile_size(sf))
            continue;
        if (is_id32le(cont_offset + 0x0C, sf, "SOUN"))
            soun_ids[soun_count++] = i;
    }
    if (soun_count == 0)
        goto fail;

    /* .trak variant: fully separate path (STHM/SSND). */
    if (is_id32le(0x20, sf, "TRAK") && is_id32le(0x24, sf, "D5ST")) {
        vgmstream = cf_df_d5_build_trak(sf, containers, soun_ids, soun_count);
        free(soun_ids);
        return vgmstream;
    }

    /* optional background track from the theme (MTHM) */
    theme_id = cf_df_d5_find_theme(sf, containers);
    has_track = (theme_id >= 0) &&
        cf_df_d5_read_theme_move(sf, containers, theme_id, &theme);

    /* individual subsong list: every SOUN, except that disk-stream fragments are folded into the
     * assembled track and not listed separately */
    listed = malloc(soun_count * sizeof(int));
    if (!listed)
        goto fail;
    for (int i = 0; i < soun_count; i++) {
        int sid = soun_ids[i];
        if (has_track && theme.disk) {
            bool fragment = false;
            for (int j = 0; j < theme.seg_count; j++) {
                if (theme.seg_souns[j] == sid) { fragment = true; break; }
            }
            if (fragment)
                continue;
        }
        listed[listed_count++] = sid;
    }

    subsongs = (has_track ? 1 : 0) + listed_count;

    target = sf->stream_index;
    if (target == 0)
        target = 1;
    if (target < 0 || target > subsongs)
        goto fail;

    if (has_track && target == 1) {
        /* subsong 1: the assembled background track */
        int trim_lo = 0, trim_hi = theme.seq_count - 1;
        if (!theme.disk) {
            /* trim leading/trailing silence (keep interspersed silence) */
            while (trim_lo <= trim_hi && cf_df_d5_soun_is_silent(sf, theme.seq[trim_lo]))
                trim_lo++;
            while (trim_hi >= trim_lo && cf_df_d5_soun_is_silent(sf, theme.seq[trim_hi]))
                trim_hi--;
            if (trim_lo > trim_hi) { trim_lo = 0; trim_hi = theme.seq_count - 1; } /* all silent: keep so it isn't empty */
        }

        vgmstream = build_d5_track(sf, theme.seq + trim_lo, trim_hi - trim_lo + 1, 0); /* .move: no loop */
        if (!vgmstream)
            goto fail;

        char basename[STREAM_NAME_SIZE];
        get_streamfile_filename(sf, basename, sizeof(basename));
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", basename);
    }
    else {
        /* individual SOUN streams */
        int piece = target - (has_track ? 1 : 0); /* 1-based index into listed[] */
        int soun_id = listed[piece - 1];

        vgmstream = build_d5_soun(sf, soun_id);
        if (!vgmstream)
            goto fail;

        char name[STREAM_NAME_SIZE], basename[STREAM_NAME_SIZE];
        cf_df_d5_lookup_name_move(sf, containers, soun_id, name, sizeof(name));
        if (name[0]) {
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", name);
        } else {
            get_streamfile_filename(sf, basename, sizeof(basename));
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%.*s#%d", STREAM_NAME_SIZE - (11 + 1 + 1), basename, piece); /* reserve '#' + 11-digit int + NUL */
        }
    }

    vgmstream->num_streams = subsongs;

    free(soun_ids);
    free(theme.seg_souns);
    free(theme.seq);
    free(listed);
    return vgmstream;

fail:
    free(soun_ids);
    free(theme.seg_souns);
    free(theme.seq);
    free(listed);
    close_vgmstream(vgmstream);
    return NULL;
}
