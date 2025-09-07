#include "meta.h"
#include "../util/endianness.h"

#define MAX_CHANNELS 4
typedef struct {
    bool loop_flag;
    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int16_t coefs[MAX_CHANNELS][16];

    int total_subsongs;

    uint32_t stream_offset;
    uint32_t stream_size;

    bool dummy;
} redspark_header_t;

static bool parse_header(redspark_header_t* h, STREAMFILE* sf, bool is_new);

/* RedSpark - Games with audio by RedSpark Ltd. (Minoru Akao) [MadWorld (Wii), Imabikisou (Wii), Mario & Luigi: Dream Team (3DS)] */
VGMSTREAM* init_vgmstream_redspark(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    bool is_new;


    /* checks*/
    uint32_t head_id = read_u32be(0x00, sf);
    if (head_id == get_id32be("RedS")) {
        is_new = true; /* M&L */
    }
    else if (head_id > 0x15800000 && head_id < 0x1B800000) {
        is_new = false; /* others: header is encrypted but in predictable ranges to fail faster (will do extra checks later) */
    }
    else {
        return NULL;
    }

    if (!check_extensions(sf, "rsd"))
        return NULL;

    redspark_header_t h = {0};
    if (!parse_header(&h, sf, is_new))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(h.channels, h.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_REDSPARK;
    vgmstream->sample_rate = h.sample_rate;
    vgmstream->num_samples = h.num_samples;
    vgmstream->loop_start_sample = h.loop_start;
    vgmstream->loop_end_sample = h.loop_end;

    vgmstream->num_streams = h.total_subsongs;
    vgmstream->stream_size = h.stream_size;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x08;

    for (int ch = 0; ch < h.channels; ch++) {
        for (int i = 0; i < 16; i++) {
            vgmstream->ch[ch].adpcm_coef[i] = h.coefs[ch][i];
        }
    }

    if (h.dummy) {
        vgmstream->num_samples = h.sample_rate;
        vgmstream->coding_type = coding_SILENCE;
    }

    if (!vgmstream_open_stream(vgmstream, sf, h.stream_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static uint32_t rotlwi(uint32_t x, uint32_t r) {
    return (x << r) | (x >> (32 - r));
}

static uint32_t decrypt_chunk(uint8_t* buf, int buf_size, uint32_t key) {
    uint32_t data;
    int pos = 0;

    if (key == 0) {
        /* initial key seems to only vary slightly between files though (doesn't seem to depend on size/name) */
        key = get_u32be(buf + 0x00) ^ get_id32be("RedS");
        data = get_u32be(buf + 0x00) ^ key;
        put_u32be(&buf[0], data);
        key = rotlwi(key, 11);

        pos = 0x04;
    }

    for (int i = pos; i < buf_size; i += 0x04) {
        key = rotlwi(key, 3) + key;
        data = get_u32be(buf + i) ^ key;
        put_u32be(buf + i, data);
    }

    return key; /* to resume decrypting if needed */
}

#define HEADER_MAX 0x3000 /* seen 0x2BD0 in one bank */

/* header is encrypted except in M&L 3DS so decrypt + handle in buffer; format
 * base 0x30 header + subheader with tables/headers depending on type */
static bool parse_header(redspark_header_t* h, STREAMFILE* sf, bool is_new) {
    uint8_t buf[HEADER_MAX];
    /* LE on 3DS */
    get_u16_t get_u16 = !is_new ? get_u16be : get_u16le;
    get_u32_t get_u32 = !is_new ? get_u32be : get_u32le;
    uint32_t curr_key = 0x00;
    int target_subsong = sf->stream_index;

    /* base header:
        00 "RedSpark"
        08 chunk size (usually data size, smaller for subspark)
        0c type/chunk? 00010000=streams, 00000000=bus/se
        10 ? (low number, usually same for multiple files)
        14 file id or null
        18 data offset
        1c bank flag
        1e 0909=stream, 0404=stream (MW) or RedSpark-within aka 'subspark' (Imabikisou), 0000=bus/se config (no audio)
        20 data size (usually file size, or header + subheader size for subsparks)
        24-30 null/reserved
    */
    int base_size = 0x30;
    if (read_streamfile(buf, 0x00, base_size,sf) != base_size)
        return false;
    if (!is_new)
        curr_key = decrypt_chunk(buf, 0x30, curr_key);

    /* get base header */
    if (get_u64be(buf + 0x00) != get_id64be("RedSpark"))
        return false;
    uint32_t data_offset = get_u32(buf + 0x18);
    uint32_t data_size = get_u32(buf + 0x20);
    int bank_flag = get_u16(buf + 0x1c);
    int type = get_u16(buf + 0x1e);

    if (data_offset >= HEADER_MAX)
        return false;


    /* get subheader and prepare offsets */
    int redspark_pos;
    int sub_offset, sub_size;
    int head_pos;
    if (data_offset == 0x30) {
        /* 'subspark', seen in a few sfx in Imabikisou (earlier version of banks) */
        /* at data offset (not encrypted):
             00 always 017F0100 (LE?)
             04-10: null
             10 file id
             13 string? size
             14 encrypted string? (ends with 0)
           at subchunk_size:
             another RedSpark with bank subflag
        */
        /* base sub-RedSpark + reset flags */
        redspark_pos = data_size;
        if (read_streamfile(buf + redspark_pos, data_size, base_size, sf) != base_size)
            return false;
        if (!is_new)
            curr_key = decrypt_chunk(buf + redspark_pos, base_size, 0); /* new header */

        /* setup rest of header (handled below) */
        uint32_t subdata_offset = get_u32(buf + redspark_pos + 0x18);
        bank_flag = get_u16(buf + redspark_pos + 0x1c);
        type = get_u16(buf + redspark_pos + 0x1e);

        data_offset = redspark_pos + subdata_offset;
        if (data_offset >= HEADER_MAX)
            return false;

        sub_offset = redspark_pos + base_size;
        sub_size = data_offset - sub_offset;
        head_pos = sub_offset;
    }
    else {
        redspark_pos = 0x00;
        sub_offset = base_size;
        sub_size = data_offset - sub_offset;
        head_pos = sub_offset;
    }

    /* read + decrypt rest of header */
    if (read_streamfile(buf + sub_offset, sub_offset, sub_size, sf) != sub_size)
        return false;
    if (!is_new)
        decrypt_chunk(buf + sub_offset, sub_size, curr_key);


    /* bus/se config */
    if (type == 0x0000) {
        /* 30 number of entries (after data offset)
            per entry
                00 channel offset (after all entries)
            per channel at the above offset
                00 channel config (similar to channel config in streams but extended) */
        vgm_logi("RedSpark: file has no audio\n");
        return false;
    }


    /* main info */
    uint32_t coef_offset;
    if (bank_flag) {
        /* bank with N subsongs (seen in M&L sfx packs and in subsparks) */
        /*  00 data size
            0c entries (only 1 is possible)
            0e entries again
            20+ table
            per entry:
                00 absolute offset to header
            per header
                00 null?
                04 stream size
                08 sample rate (null in subspark)
                0c nibbles (old) or samples (new), (null in subspark so uses loops)
                10 stream offset (from data_offset)
                14 loop end (num samples if non-looped)
                18 loop start of -1 it not looped
                20 config?
                24 config?
                28 coefs + hists
                5c channel config?
        */

        h->total_subsongs = get_u16(buf + head_pos + 0x0c);
        if (!check_subsongs(&target_subsong, h->total_subsongs))
            return false;
   
        int target_pos = head_pos + 0x20 + (target_subsong - 1) * 0x04;
        if (target_pos + 0x04 >= HEADER_MAX)
            return false;
        target_pos = get_u32(buf + target_pos) + redspark_pos;
        if (target_pos + 0x70 >= HEADER_MAX)
            return false;

        h->stream_size      = get_u32(buf + target_pos + 0x04);
        h->sample_rate      = get_u32(buf + target_pos + 0x08);
        h->num_samples      = get_u32(buf + target_pos + 0x0c);
        h->stream_offset    = get_u32(buf + target_pos + 0x10) + data_offset;
        h->loop_end         = get_u32(buf + target_pos + 0x14);
        h->loop_start       = get_u32(buf + target_pos + 0x18);
        coef_offset         = target_pos + 0x28;

        h->channels = 1;
        h->loop_flag = 0; // (h->loop_start != -1); // TODO: many files sound kind of odd
        if (h->num_samples == 0)
            h->num_samples = h->loop_end;
        // seems correct based on MadWorld, not 100% sure in Imabikisou
        if (h->sample_rate == 0)
            h->sample_rate = 24000;
        
        /* empty entry */
        if (h->stream_size == 0)
            h->dummy = true;
    }
    else {
        /* stream */
        /*  00 data size (after data offset)
            04 null
            08 null
            0c sample rate
            10 frames (old) or samples (new)
            14 some chunk? (0x10000, 0xc000)
            18 null
            1c null
            1e channels (usually 1-2, sometimes 4 in MW)
            1f num loop cues (2 == loop)
            20 cues again?
            21 volume?
            22 null
            24+ variable
            per channel
                00 config per channel, size 0x08 (number/panning/volume/etc?)
            per loop point
                00 chunk size?
                04 value
            per channel
                00 dsp coefs + hists (size 0x2E)
            if cues:
                00 offset?
                04 null
            per cue: 
                00 name size
                01 string ("Loop Start" / "Loop End")
        */

        h->sample_rate = get_u32(buf + head_pos + 0x0c);
        h->num_samples = get_u32(buf + head_pos + 0x10);
        h->channels = get_u8(buf + head_pos + 0x1e);
        int loop_cues = get_u8(buf + head_pos + 0x1f);

        head_pos += 0x24;

        /* just in case to avoid bad reads outside buf */
        if (h->channels > MAX_CHANNELS)
            return false;
        head_pos += h->channels * 0x08;

        h->loop_flag = (loop_cues != 0);
        if (h->loop_flag) {
            /* only two cue points */
            if (loop_cues != 0 && loop_cues != 2)
                return false;
            h->loop_start = get_u32(buf + head_pos + 0x04);
            h->loop_end = get_u32(buf + head_pos + 0x0c);
            head_pos += 0x10;
        }

        coef_offset = head_pos;
        h->stream_offset = data_offset;
        h->stream_size = data_size;
    }

    /* coefs from decrypted buf (could read hist but it's 0 in DSPs) */
    for (int ch = 0; ch < h->channels; ch++) {
        for (int i = 0; i < 16; i++) {
            h->coefs[ch][i] = get_u16(buf + coef_offset + 0x2e * ch + i * 2);
        }
    }

    /* fixes */
    if (!is_new) {
        h->loop_end += 1;

        if (bank_flag) {
            h->num_samples /= 2 * 0x8;
            h->loop_start /= 2 * 0x8;
            h->loop_end /= 2 * 0x8;
        }
        
        h->num_samples *= 14;
        h->loop_start *= 14;
        h->loop_end *= 14;

        if (h->loop_end > h->num_samples) /* needed for some files */
            h->loop_end = h->num_samples;
    }
    /* new + bank may need +1 */

    return true;
}
