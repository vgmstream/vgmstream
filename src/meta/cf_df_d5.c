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
 * - SOUN payload base H = container + 0x08. Codec is fixed per stream and selected by H+0x1a
 *   (==1 -> v4.0 ADPCM), else H+0x18 (==0 -> v4.1 DPCM), else IMA. Sample rate at H+0x1c (native
 *   11025/22050/44100). Block count at H+0x28; block-offset table at H+0x2c (input offsets relative
 *   to H). Codec state resets every block;
 *
 * MTHM is the v5 analog of v4's container-1 loop block: a fixed-size order region (order_count u16
 * @T+0x1c, order list u16[] @T+0x1e, 1-based indices) followed by a segment array (segment_count
 * u32 @T+0x222, array @T+0x226 stride 0x22). Each segment descriptor holds the absolute SOUN
 * container id @+0x0c and a Pascal-string name @+0x12.
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

/* Resolve a SOUN's display name. Priority: (1) the Pascal-string name on a theme segment descriptor
 * that references it; else (2) the MSND sound director. MSND ids are scene-relative: a multi-scene
 * movie repeats MHED(+0)/MTHM(+1)/MSND(+2)/MTRG(+3) per scene, so an MSND at container index m has
 * scene base m-2 and its entries map to absolute SOUN base+rel. Leaves dst empty if unnamed. */
static void cf_df_d5_lookup_name(STREAMFILE* sf, int containers, int soun_id, char* dst, int dst_size) {
    dst[0] = '\0';

    /* (1) theme segment-descriptor name */
    for (int i = 0; i < containers; i++) {
        off_t coff = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (coff <= 0 || coff + 0x30 > get_streamfile_size(sf))
            continue;
        if (!is_id32le(coff + 0x0C, sf, "MTHM"))
            continue;

        off_t T = coff + 0x08;
        uint32_t sc = read_u32le(T + DF_D5_THEME_SEG_COUNT, sf);
        if (sc == 0 || sc > DF_MAX_CHUNKS)
            continue;
        for (uint32_t s = 0; s < sc; s++) {
            off_t desc = T + DF_D5_THEME_SEG + (off_t)s * DF_D5_THEME_SEG_STRIDE;
            if ((int)read_u32le(desc + DF_D5_THEME_SEG_SOUN, sf) != soun_id)
                continue;
            int len = read_u8(desc + DF_D5_THEME_SEG_NAME, sf);
            if (len <= 0)
                break;
            if (len > dst_size - 1)
                len = dst_size - 1;
            if (desc + DF_D5_THEME_SEG_NAME + 1 + len > get_streamfile_size(sf))
                break;
            for (int c = 0; c < len; c++)
                dst[c] = read_u8(desc + DF_D5_THEME_SEG_NAME + 1 + c, sf);
            dst[len] = '\0';
            return;
        }
    }

    /* (2) MSND sound director (scene-relative) */
    int current_scene_base = 0;
    for (int i = 0; i < containers; i++) {
        off_t coff = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (coff <= 0 || coff + 0x30 > get_streamfile_size(sf))
            continue;

        if (is_id32le(coff + 0x0C, sf, "MHED")) {
            current_scene_base = i;
        }
        if (!is_id32le(coff + 0x0C, sf, "MSND"))
            continue;

        int scene_base = current_scene_base;

        off_t H = coff + 0x08; /* MSND payload */
        int n = read_u32le(H + 0x18, sf);
        if (n <= 0 || n > DF_MAX_CHUNKS)
            continue;

        for (int k = 0; k < n; k++) {
            off_t e = H + 0x28 + (off_t)k * 0x30;
            if (e + 0x09 > get_streamfile_size(sf))
                break;
            if (scene_base + read_u16le(e + 0x02, sf) != soun_id)
                continue;

            int len = read_u8(e + 0x08, sf);
            if (len > dst_size - 1)
                len = dst_size - 1;
            if (e + 0x09 + len > get_streamfile_size(sf))
                return;
            for (int c = 0; c < len; c++)
                dst[c] = read_u8(e + 0x09 + c, sf);
            dst[len] = '\0';
            return;
        }
    }
}

static bool cf_df_d5_soun_is_silent(STREAMFILE* sf, int soun_id) {
    off_t pos = read_u32le(DF_HEADER_SIZE + (off_t)soun_id * 0x04, sf);
    if (pos <= 0 || pos + 0x30 > get_streamfile_size(sf))
        return false;

    off_t H = pos + 0x08;
    uint32_t cont_size = read_u32le(pos + 0x04, sf);
    int sel18 = read_u16le(H + 0x18, sf);
    int sel1a = read_u16le(H + 0x1a, sf);
    int count = read_u32le(H + 0x28, sf);
    off_t table = H + 0x2c;
    if (count <= 0 || count > DF_MAX_CHUNKS)
        return false;

    int coding = (sel1a == 1) ? coding_CF_DF_ADPCM_v5
               : (sel18 == 0) ? coding_CF_DF_DPCM_V41
                              : coding_CF_DF_IMA_v5;
    if (coding == coding_CF_DF_IMA_v5)
        return false; /* unknown silence pattern; keep */

    uint8_t buf[0x1000];
    for (int k = 0; k < count; k++) {
        uint32_t rel = read_u32le(table + (off_t)k * 0x04, sf);
        uint32_t next_rel = (k + 1 < count) ? read_u32le(table + (off_t)(k + 1) * 0x04, sf) : cont_size;
        if (next_rel < rel || next_rel > cont_size)
            return false;

        off_t p = H + rel;
        int32_t left = (int32_t)(next_rel - rel);
        while (left > 0) {
            int to_read = left > (int)sizeof(buf) ? (int)sizeof(buf) : left;
            int got = read_streamfile(buf, p, to_read, sf);
            if (got <= 0)
                return false;
            for (int i = 0; i < got; i++) {
                uint8_t b = buf[i];
                if (coding == coding_CF_DF_DPCM_V41) {
                    if (b != 0x00 && b != 0x80)
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

    off_t pos = read_u32le(DF_HEADER_SIZE + (off_t)soun_id * 0x04, sf);
    if (pos <= 0 || pos + 0x30 > get_streamfile_size(sf))
        return NULL;
    if (!is_id32le(pos + 0x0C, sf, "SOUN"))
        return NULL;

    off_t H = pos + 0x08;
    uint32_t cont_size = read_u32le(pos + 0x04, sf);
    int sel18 = read_u16le(H + 0x18, sf);
    int sel1a = read_u16le(H + 0x1a, sf);
    int rate  = read_u32le(H + 0x1c, sf);
    int count = read_u32le(H + 0x28, sf);
    off_t table = H + 0x2c;

    if (rate != 11025 && rate != 22050 && rate != 44100)
        return NULL;
    if (count <= 0 || count > DF_MAX_CHUNKS)
        return NULL;
    if (table + (off_t)count * 0x04 > H + cont_size)
        return NULL;

    int coding;
    if (sel1a == 1)
        coding = coding_CF_DF_ADPCM_v5;
    else if (sel18 == 0)
        coding = coding_CF_DF_DPCM_V41;
    else
        coding = coding_CF_DF_IMA_v5;

    int32_t num_samples = 0;
    for (int k = 0; k < count; k++) {
        uint32_t rel = read_u32le(table + (off_t)k * 0x04, sf);
        uint32_t next_rel = (k + 1 < count) ? read_u32le(table + (off_t)(k + 1) * 0x04, sf) : cont_size;
        if (next_rel < rel || next_rel > cont_size)
            return NULL;
        int B = (int)(next_rel - rel);
        off_t block_data = H + rel;

        if (coding == coding_CF_DF_IMA_v5) {
            if (B < 3) continue;
            int step = read_u8(block_data + 0x02, sf);
            num_samples += (step > 0x58) ? 0 : (1 + 2 * (B - 3));
        }
        else if (coding == coding_CF_DF_ADPCM_v5) {
            if (B < 1) continue;
            num_samples += cf_df_v5_get_samples(sf, block_data, B);
        }
        else { /* v4.1 */
            num_samples += B;
        }
    }
    if (num_samples <= 0)
        return NULL;

    vgmstream = allocate_vgmstream(1, 0);
    if (!vgmstream) return NULL;

    vgmstream->meta_type   = meta_CF_DF_D5;
    vgmstream->sample_rate = rate;
    vgmstream->num_samples = num_samples;
    vgmstream->stream_size = cont_size;
    vgmstream->coding_type = coding;
    vgmstream->layout_type = layout_blocked_cf_df_v5;

    if (!vgmstream_open_stream(vgmstream, sf, table)) {
        close_vgmstream(vgmstream);
        return NULL;
    }
    return vgmstream;
}

// Find the first MTHM container that carries a usable theme (segment_count > 0).
static int cf_df_d5_find_theme(STREAMFILE* sf, int containers) {
    for (int i = 0; i < containers; i++) {
        off_t coff = read_u32le(DF_HEADER_SIZE + (off_t)i * 0x04, sf);
        if (coff <= 0 || coff + 0x30 > get_streamfile_size(sf))
            continue;
        if (!is_id32le(coff + 0x0C, sf, "MTHM"))
            continue;
        off_t T = coff + 0x08;
        uint32_t sc = read_u32le(T + DF_D5_THEME_SEG_COUNT, sf);
        if (sc > 0 && sc <= DF_MAX_CHUNKS)
            return i;
    }
    return -1;
}

/* Read a theme into: seg_souns[] (every segment's SOUN id, in array order) and seq[] (the playback
 * sequence of SOUN ids). disk_stream movies (segment_count > order_count) play the whole segment
 * array; otherwise the sequence follows the 1-based order list. Returns false (no track) if the
 * theme is unparseable or any reference doesn't resolve to a real SOUN -- so the file degrades to a
 * plain SOUN list instead of failing or assembling garbage. */
static bool cf_df_d5_read_theme(STREAMFILE* sf, int containers, int theme_id,
        int** out_seg_souns, int* out_seg_count, int** out_seq, int* out_seq_count, bool* out_disk) {
    *out_seg_souns = NULL; *out_seg_count = 0;
    *out_seq = NULL;       *out_seq_count = 0;
    *out_disk = false;

    off_t T = read_u32le(DF_HEADER_SIZE + (off_t)theme_id * 0x04, sf) + 0x08;
    int order_count = read_u16le(T + DF_D5_THEME_ORDER_COUNT, sf);
    uint32_t seg_count = read_u32le(T + DF_D5_THEME_SEG_COUNT, sf);
    if (seg_count == 0 || seg_count > DF_MAX_CHUNKS)
        return false;
    if (order_count < 0 || order_count > DF_MAX_CHUNKS)
        return false;

    int* segs = malloc((size_t)seg_count * sizeof(int));
    if (!segs) return false;

    for (uint32_t s = 0; s < seg_count; s++) {
        off_t desc = T + DF_D5_THEME_SEG + (off_t)s * DF_D5_THEME_SEG_STRIDE;
        int soun = read_u32le(desc + DF_D5_THEME_SEG_SOUN, sf);
        off_t soff = (soun >= 0 && soun < containers)
                   ? read_u32le(DF_HEADER_SIZE + (off_t)soun * 0x04, sf) : 0;
        if (soun < 0 || soun >= containers || soff <= 0 ||
                soff + 0x30 > get_streamfile_size(sf) || !is_id32le(soff + 0x0C, sf, "SOUN")) {
            free(segs);
            return false;
        }
        segs[s] = soun;
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
            int e = read_u16le(T + DF_D5_THEME_ORDER + (off_t)i * 0x02, sf); /* 1-based */
            if (e < 1 || (uint32_t)e > seg_count) { free(seq); free(segs); return false; }
            seq[i] = segs[e - 1];
        }
        seq_count = order_count;
    }

    *out_seg_souns = segs; *out_seg_count = (int)seg_count;
    *out_seq = seq;        *out_seq_count = seq_count;
    *out_disk = disk;
    return true;
}

/* Assemble the background track: play the given SOUN sequence once via the segmented layout, like
 * the v4 path's build_segmented. Each step gets its own decoder (so a repeated SOUN is a distinct
 * segment, never a double free). */
static VGMSTREAM* build_d5_track(STREAMFILE* sf, int* seq, int count) {
    VGMSTREAM* v = NULL;
    segmented_layout_data* data = init_layout_segmented(count);
    if (!data) goto fail;

    for (int i = 0; i < count; i++) {
        VGMSTREAM* seg = build_d5_soun(sf, seq[i]);
        if (!seg) goto fail;
        data->segments[i] = seg;
    }

    if (!setup_layout_segmented(data))
        goto fail;

    v = allocate_segmented_vgmstream(data, 0, -1, -1);
    if (!v) goto fail;
    return v;

fail:
    free_layout_segmented(data);
    close_vgmstream(v);
    return NULL;
}

VGMSTREAM* init_vgmstream_cf_df_d5(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int* soun_ids = NULL;
    int* seg_souns = NULL;
    int* seq = NULL;
    int* listed = NULL;

    /* checks */
    if (!( (is_id32le(0x20, sf, "MOVE") && is_id32le(0x24, sf, "D5ME")) ||
           (is_id32le(0x20, sf, "TRAK") && is_id32le(0x24, sf, "D5ST")) ))
        return NULL;
    if (read_u32le(0x04, sf) != get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "move,trak"))
        return NULL;

    int containers = read_u32le(0x14, sf);
    if (containers <= 0 || containers > INT16_MAX)
        return NULL;

    /* collect SOUN containers */
    soun_ids = malloc(containers * sizeof(int));
    if (!soun_ids) goto fail;

    int soun_count = 0;
    for (int i = 0; i < containers; i++) {
        off_t coff = read_u32le(DF_HEADER_SIZE + i * 0x04, sf);
        if (coff <= 0 || coff + 0x30 > get_streamfile_size(sf))
            continue;
        if (is_id32le(coff + 0x0C, sf, "SOUN"))
            soun_ids[soun_count++] = i;
    }
    if (soun_count == 0)
        goto fail;

    /* optional background track from the theme (MTHM) */
    int seg_count = 0, seq_count = 0;
    bool disk_stream = false;
    int theme_id = cf_df_d5_find_theme(sf, containers);
    bool has_track = (theme_id >= 0) &&
        cf_df_d5_read_theme(sf, containers, theme_id, &seg_souns, &seg_count, &seq, &seq_count, &disk_stream);

    /* individual subsong list: every SOUN, except that disk-stream fragments are folded into the
     * assembled track and not listed separately */
    listed = malloc(soun_count * sizeof(int));
    if (!listed) goto fail;
    int listed_count = 0;
    for (int i = 0; i < soun_count; i++) {
        int sid = soun_ids[i];
        if (has_track && disk_stream) {
            bool fragment = false;
            for (int j = 0; j < seg_count; j++) {
                if (seg_souns[j] == sid) { fragment = true; break; }
            }
            if (fragment)
                continue;
        }
        listed[listed_count++] = sid;
    }

    int subsongs = (has_track ? 1 : 0) + listed_count;

    int target = sf->stream_index;
    if (target == 0)
        target = 1;
    if (target < 0 || target > subsongs)
        goto fail;

    if (has_track && target == 1) {
        /* subsong 1: the assembled background track */
        int lo = 0, hi = seq_count - 1;
        if (!disk_stream) {
            /* trim leading/trailing silence (keep interspersed silence) */
            while (lo <= hi && cf_df_d5_soun_is_silent(sf, seq[lo]))
                lo++;
            while (hi >= lo && cf_df_d5_soun_is_silent(sf, seq[hi]))
                hi--;
            if (lo > hi) { lo = 0; hi = seq_count - 1; } /* all silent: keep so it isn't empty */
        }

        vgmstream = build_d5_track(sf, seq + lo, hi - lo + 1);
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
        cf_df_d5_lookup_name(sf, containers, soun_id, name, sizeof(name));
        if (name[0]) {
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", name);
        } else {
            get_streamfile_filename(sf, basename, sizeof(basename));
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%.*s#%d", STREAM_NAME_SIZE - (11 + 1 + 1), basename, piece); /* reserve '#' + 11-digit int + NUL */
        }
    }

    vgmstream->num_streams = subsongs;

    free(soun_ids);
    free(seg_souns);
    free(seq);
    free(listed);
    return vgmstream;

fail:
    free(soun_ids);
    free(seg_souns);
    free(seq);
    free(listed);
    close_vgmstream(vgmstream);
    return NULL;
}