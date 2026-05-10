#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <limits.h>
#include "meta.h"
#include "adx_keys.h"
#include "../coding/coding.h"
#include "../util/cri_keys.h"
#include "../util/companion_files.h"


#ifdef VGM_DEBUG_OUTPUT
  //#define ADX_BRUTEFORCE
#endif


static bool find_adx_key(STREAMFILE* sf, uint8_t type, uint16_t* xor_start, uint16_t* xor_mult, uint16_t* xor_add, uint16_t subkey);

VGMSTREAM* init_vgmstream_adx(STREAMFILE* sf) {
    return init_vgmstream_adx_subkey(sf, 0);
}

/* ADX - CRI Middleware format */
VGMSTREAM* init_vgmstream_adx_subkey(STREAMFILE* sf, uint16_t subkey) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, hist_offset = 0;
    int loop_flag = 0, channels, sample_rate;
    int32_t num_samples, loop_start_sample = 0, loop_end_sample = 0;
    uint16_t cutoff;
    uint16_t version;
    uint16_t xor_start = 0, xor_mult = 0, xor_add = 0;
    meta_t header_type;
    coding_t coding_type;


    /* checks*/
    if (read_u16be(0x00,sf) != 0x8000)
        return NULL;

    /* .adx: standard
     * .adp: Headhunter (DC)
     * .sfa: Softdec2 video's extension for ADX audio tracks (before importing) */
    if (!check_extensions(sf,"adx,adp,sfa"))
        return NULL;

    start_offset = read_u16be(0x02,sf) + 0x04;

    uint8_t encoding_type = read_u8(0x04, sf);
    switch (encoding_type) {
        case 0x02:
            coding_type = coding_CRI_ADX_fixed; /* unused/encoder only */
            break;
        case 0x03:
            coding_type = coding_CRI_ADX;
            break;
        case 0x04:
            coding_type = coding_CRI_ADX_exp; /* unused/encoder only */
            break;
        default: /* 0x10 is AHX for DC, 0x11 is AHX */
            return NULL;
    }

    /* ADX encoders can't set this value, but is honored by ADXPlay if changed and multiple of 0x12,
     * though output is unusual and may not be fully supported (works in mono so not an interleave)
     * Later versions of the decoder just use constant 0x12 ignoring it, though. */
    uint8_t frame_size = read_u8(0x05, sf);
    if (frame_size != 0x12)
        return NULL;
    uint8_t bits_per_sample = read_u8(0x06,sf);
    if (bits_per_sample != 4)
        return NULL;

    channels = read_u8(0x07,sf); /* older ADX (adxencd) up to 2ch, newer ADX (criatomencd) up to 8 */
    if (channels > 8)
        return NULL;
    sample_rate = read_s32be(0x08,sf);
    num_samples = read_s32be(0x0c,sf);
    cutoff = read_u16be(0x10,sf); /* high-pass cutoff frequency, always 500 */
    version = read_u16be(0x12,sf); /* version + revision, originally read as separate */

    /* encryption */
    if (version == 0x0408) {
        if (!find_adx_key(sf, 8, &xor_start, &xor_mult, &xor_add, 0)) {
            vgm_logi("ADX: decryption keystring not found\n");
        }
        coding_type = coding_CRI_ADX_enc_8;
        version = 0x0400;
    }
    else if (version == 0x0409) {
        if (!find_adx_key(sf, 9, &xor_start, &xor_mult, &xor_add, subkey)) {
            vgm_logi("ADX: decryption keycode not found\n");
        }
        coding_type = coding_CRI_ADX_enc_9;
        version = 0x0400;
    }

    /* version + extra data */
    if (version == 0x0300) {  /* early ADX (~1998) [Grandia (SAT), Baroque (SAT)] */
        size_t base_size = 0x14, loops_size = 0x18;

        header_type = meta_ADX_03;

        /* no sample history */

        if (start_offset - 0x06 >= base_size + loops_size) { /* enough space for loop info? */
            off_t loops_offset = base_size;

            /* 0x00 (2): initial loop padding (the encoder adds a few blank samples so loop start is block-aligned; max 31)
             *  ex. loop_start=12: enc_start=32, padding=20 (32-20=12); loop_start=35: enc_start=64, padding=29 (64-29=35)
             * 0x02 (2): loop flag? (always 1) */
            loop_flag           = read_s32be(loops_offset+0x04,sf) != 0; /* loop count + loop type? (always 1) */
            loop_start_sample   = read_s32be(loops_offset+0x08,sf);
            //loop_start_offset = read_u32be(loops_offset+0x0c,sf);
            loop_end_sample     = read_s32be(loops_offset+0x10,sf);
            //loop_end_offset   = read_u32be(loops_offset+0x14,sf);
        }
    }
    else if (version == 0x0400) {  /* common */
        size_t base_size = 0x18, hist_size, ainf_size = 0, loops_size = 0x18;
        off_t ainf_offset;

        header_type = meta_ADX_04;

        hist_offset = base_size; /* always present but often blank */
        hist_size = (channels > 1 ? 0x04 * channels : 0x04 + 0x04); /* min is 0x8, even in 1ch files */

        ainf_offset = base_size + hist_size + 0x04; /* not seen with >2ch though */
        if (is_id32be(ainf_offset+0x00,sf, "AINF")) {
            ainf_size = read_u32be(ainf_offset+0x04,sf);
        }

        if (start_offset - ainf_size - 0x06 >= hist_offset + hist_size + loops_size) {  /* enough space for loop info? */
            off_t loops_offset = base_size + hist_size;

            /* 0x00 (2): initial loop padding (the encoder adds a few blank samples so loop start is block-aligned; max 31)
             *  ex. loop_start=12: enc_start=32, padding=20 (32-20=12); loop_start=35: enc_start=64, padding=29 (64-29=35)
             * 0x02 (2): loop flag? (always 1) */
            loop_flag           = read_s32be(loops_offset+0x04,sf) != 0; /* loop count + loop type? (always 1) */
            loop_start_sample   = read_s32be(loops_offset+0x08,sf);
          //loop_start_offset   = read_u32be(loops_offset+0x0c,sf);
            loop_end_sample     = read_s32be(loops_offset+0x10,sf);
          //loop_end_offset     = read_u32be(loops_offset+0x14,sf);
        }

        /* AINF header info (may be inserted by CRI's tools but is rarely used)
         *  Can also start right after the loop points (base_size + hist_size + loops_size)
         * 0x00 (4): "AINF"
         * 0x04 (4): size
         * 0x08 (10): str_id
         * 0x18 (2): volume (0=base/max?, negative=reduce)
         * 0x1c (2): pan l
         * 0x1e (2): pan r (0=base, max +-128) */

        /* CINF header info (very rare, found after loops) [Sakura Taisen 3 (PS2)]
         * 0x00 (4): "CINF"
         * 0x04 (4): size
         * 0x08 (4): "ASO ", unknown
         * 0x28 (4): "SND ", unknown
         * 0x48 (-): file name, null terminated
         */
    }
    else if (version == 0x0500) {  /* found in some SFD: Buggy Heat, appears to have no loop */
        header_type = meta_ADX_05;
    }
    else { /* not a known/supported version signature */
        return NULL;
    }


    /* CRI mainly checks value 0x8000 at 0x00, and memcmps this, but offset is right before data start
     * usually aligned to 0x100/0x800/0x1000, so do other checks first to avoid seeking back and forth */
    uint8_t cri_str[0x06] = {0};
    read_streamfile(cri_str, start_offset - 0x06, sizeof(cri_str), sf);
    if (memcmp(cri_str, "(c)CRI", sizeof(cri_str)) != 0)
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->codec_config = version;
    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;
    vgmstream->meta_type = header_type;


    /* calculate filter coefficients */
    if (coding_type == coding_CRI_ADX_fixed) {
        /* standard XA coefs * (2 << 11) */
        for (int i = 0; i < channels; i++) {
            vgmstream->ch[i].adpcm_coef[0] = 0x0000;
            vgmstream->ch[i].adpcm_coef[1] = 0x0000;
            vgmstream->ch[i].adpcm_coef[2] = 0x0F00;
            vgmstream->ch[i].adpcm_coef[3] = 0x0000;
            vgmstream->ch[i].adpcm_coef[4] = 0x1CC0;
            vgmstream->ch[i].adpcm_coef[5] = 0xF300;
            vgmstream->ch[i].adpcm_coef[6] = 0x1880;
            vgmstream->ch[i].adpcm_coef[7] = 0xF240;
        }
    }
    else {
        /* coefs from cutoff frequency (some info from decomps, uses floats but no diffs if using doubles due to rounding) */
        float x, y, z, a, b, c;

        x = cutoff;
        y = sample_rate;
        z = cosf(2.0 * M_PI * x / y); /* 2.0 * M_PI: 6.28318548202515f (decomp) */

        a = M_SQRT2 - z;    /* M_SQRT2: 1.41421353816986f (decomp) */
        b = M_SQRT2 - 1.0;  /* M_SQRT2 - 1: 0.414213538169861f (decomp) */
        c = (a - sqrtf((a + b) * (a - b))) / b; /* this seems calculated with a custom algorithm */

        int16_t coef1 = (short)(c * 8192);
        int16_t coef2 = (short)(c * c * -4096);

        for (int i = 0; i < channels; i++) {
            vgmstream->ch[i].adpcm_coef[0] = coef1;
            vgmstream->ch[i].adpcm_coef[1] = coef2;
        }
    }

    /* init decoder */
    for (int i = 0; i < channels; i++) {
        /* 2 hist shorts per ch, corresponding to the very first original sample repeated (verified with CRI's encoders).
         * Not vital as their effect is small, after a few samples they don't matter, and most songs start in silence. */
        if (hist_offset) {
            vgmstream->ch[i].adpcm_history1_32 = read_s16be(hist_offset + i*4 + 0x00,sf);
            vgmstream->ch[i].adpcm_history2_32 = read_s16be(hist_offset + i*4 + 0x02,sf);
        }

        if (coding_type == coding_CRI_ADX_enc_8 || coding_type == coding_CRI_ADX_enc_9) {
            vgmstream->ch[i].adx_xor = xor_start;
            vgmstream->ch[i].adx_mult = xor_mult;
            vgmstream->ch[i].adx_add = xor_add;

            for (int j = 0; j < i; j++)
                adx_next_key(&vgmstream->ch[i]);
        }
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}


//#define ADX_PRINT_KEY_INFO 1
#ifdef ADX_PRINT_KEY_INFO
/* derive and print all keys in the list, quick validity test */
static void print_key_info(adxkey_info* key, uint8_t type, uint16_t subkey) {
    uint16_t test_xor, test_mul, test_add;

    uint16_t xor = key->start;
    uint16_t mul = key->mult;
    uint16_t add = key->add;
    if (type == 8 && key->key8) {
        cri_key8_derive(key->key8, &test_xor, &test_mul, &test_add);
        VGM_LOG("key8: pre=%04x %04x %04x vs calc=%04x %04x %04x = %s (\"%s\")\n",
                xor,mul,add, test_xor,test_mul,test_add,
                xor==test_xor && mul==test_mul && add==test_add ? "ok" : "ko", key->key8);
    }
    else if (type == 9 && key->key9) {
        cri_key9_derive(key->key9, subkey, &test_xor, &test_mul, &test_add);
        VGM_LOG("key9: pre=%04x %04x %04x vs calc=%04x %04x %04x = %s (%"PRIu64")\n",
                xor,mul,add, test_xor,test_mul,test_add,
                xor==test_xor && mul==test_mul && add==test_add ? "ok" : "ko", key->key9);
    }
}
#endif

static bool read_external_key(STREAMFILE* sf, uint8_t type, uint16_t* xor_start, uint16_t* xor_mult, uint16_t* xor_add, uint16_t subkey) {
    uint8_t keybuf[0x40+1] = {0}; // known max ~0x30, +1 extra null for keystrings

    /* handle type8 keystrings, key9 keycodes and derived keys */
    size_t key_size = read_key_file(keybuf, sizeof(keybuf) - 1, sf);
    if (key_size <= 0) 
        return false;

    bool is_keystring = false;
    if (type == 8) {
        is_keystring = cri_key8_valid_keystring(keybuf, key_size);
    }
    else if (type == 9) {
        is_keystring = cri_key9_valid_keystring(keybuf, key_size);
    }

    if (key_size == 0x06 && !is_keystring) {
        *xor_start = get_u16be(keybuf + 0x00);
        *xor_mult  = get_u16be(keybuf + 0x02);
        *xor_add   = get_u16be(keybuf + 0x04);
        return true;
    }

    if (type == 8 && is_keystring) {
        const char* keystring = (const char*)keybuf;
        cri_key8_derive(keystring, xor_start, xor_mult, xor_add);
        return true;
    }

    if (type == 9 && is_keystring) {
        const char* keystring = (const char*)keybuf;
        uint64_t keycode = strtoull(keystring, NULL, 10);
        cri_key9_derive(keycode, subkey, xor_start, xor_mult, xor_add);
        return true;
    }

    if (type == 9 && key_size == 0x08) {
        uint64_t keycode = get_u64be(keybuf);
        cri_key9_derive(keycode, subkey, xor_start, xor_mult, xor_add);
        return true;
    }

    if (type == 9 && key_size == 0x08+0x02) {
        uint64_t file_keycode = get_u64be(keybuf+0x00);
        uint16_t file_subkey  = get_u16be(keybuf+0x08);
        cri_key9_derive(file_keycode, file_subkey, xor_start, xor_mult, xor_add);
        return true;
    }

    return false;
}


#define ADX_KEY_MIN_TEST_FRAMES 128     // not including blanks; typically 8-32 is enough but add some more just in case
#define ADX_KEY_SCALES_GROWTH 1.5f
#define ADX_KEY_TEST_BUFFER_SIZE 0x2000 // enough for min frames + extra for empty
#define ADX_KEY_BLANK_FRAME_MARK 0xFFFF // valid but unlikely scale

typedef struct {
    uint16_t* scales;
    int bruteframe_start;
    int bruteframe_count;
    uint16_t keymask;
} adx_keytest_t;

/* read N scales, starting from first non-zero frame (not useful for detection) */
static bool setup_adx_keytest(adx_keytest_t* keytest, uint8_t type, STREAMFILE* sf) {
    const int frame_size = 0x12;
    static const uint8_t frame_zeroes[0x12] = {0};
    uint8_t buf[ADX_KEY_TEST_BUFFER_SIZE];

    uint32_t offset = read_u16be(0x02, sf) + 0x04;
    int channels    =    read_u8(0x07, sf);
    int num_samples = read_s32be(0x0c, sf);

    int frame_count = (num_samples + 31) / 32 * channels; // 32 samples per frame

    int buf_pos = sizeof(buf); // force initial read
    int bruteframe_start = 0; // blank frames until first actual frame
    int bruteframe_count = 0; // total frames in 'scales' (including blank frames)
    int bruteframe_valid = 0; // regular non-blank frames


    // defaults with extra for blanks, may realloc as needed
    int max_scales = ADX_KEY_MIN_TEST_FRAMES * ADX_KEY_SCALES_GROWTH;
    uint16_t* scales = malloc(max_scales * sizeof(uint16_t));
    if (!scales) return false;

    keytest->scales = scales; // for early returns

    // read valid scales
    for (int i = 0; i < frame_count; i++) {

        // new chunk to extract scales
        if (buf_pos + frame_size >= sizeof(buf)) {
            int buf_frames = sizeof(buf) / frame_size * frame_size;
            int bytes = read_streamfile(buf, offset, buf_frames, sf);
            offset += bytes;
            buf_pos = 0;
        }

        bool is_blank = memcmp(buf + buf_pos, frame_zeroes, frame_size) == 0;
        if (is_blank && bruteframe_count == 0) {
            bruteframe_start++; // skips until first non-empty frame (for songs that start with silence)
        }
        else if (is_blank) {
            scales[bruteframe_count] = ADX_KEY_BLANK_FRAME_MARK;
            bruteframe_count++;
        }
        else {
            scales[bruteframe_count] = get_u16be(buf + buf_pos);
            bruteframe_count++;
            bruteframe_valid++;
        }

        buf_pos += frame_size;

        if (bruteframe_valid >= ADX_KEY_MIN_TEST_FRAMES)
            break;

        // rarely there may be empty frames mixed with regular frames, so keep reading more
        if (bruteframe_count >= max_scales) {
            max_scales = max_scales * ADX_KEY_SCALES_GROWTH;

            uint16_t* temp_scales = realloc(scales, max_scales * sizeof(uint16_t));
            if (!temp_scales) return false;
            scales = temp_scales;
        }
    }

    //;VGM_LOG("ADX: bruteframes: count=%i, start=%i, others=%i\n", bruteframe_count, bruteframe_start, bruteframe_others, bruteframe_blanks);

    keytest->scales = scales; // in case or reallocs
    keytest->bruteframe_start = bruteframe_start;
    keytest->bruteframe_count = bruteframe_count;

    // setup test mask (used to check high bits for 13-bit scales)
    if (type == 8) {
        keytest->keymask = 0x6000;
    }
    else if (type == 9) {
        // smarter XOR as seen in PSO2. The scale is technically 13 bits, but the maximum value assigned by
        // the encoder is 0x1000. This is written to the ADX file as 0xFFF, leaving the high bit empty,
        // which is used to validate a key
        keytest->keymask = 0x1000;
    }

    return true;
}

/* test current XOR (global for all channels) vs prescales + scales.
 * Some of the ADX scales's upper bits aren't used, so after XORing the should partially match the XOR.
 * Empty frames's scales aren't XORed (ignored), while regular frames may uncommonly use scale 0. */
static bool validate_adx_key(adx_keytest_t* keytest, uint16_t xor, uint16_t mul, uint16_t add) {
    uint16_t* scales = keytest->scales;
    uint16_t keymask = keytest->keymask;

    for (int i = 0; i < keytest->bruteframe_start; i++) {
        xor = xor * mul + add; // skip initial blanks
    }

    for (int i = 0; i < keytest->bruteframe_count; i++) {
        if ((scales[i] & keymask) != (xor & keymask) && scales[i] != ADX_KEY_BLANK_FRAME_MARK)
            return false;
        xor = xor * mul + add;
    }

    // all scales are valid, key is good
    return true;
}

/* ADX key detection works by reading XORed ADPCM 16-bit scales in frames, and un-XORing with keys in
 * a list. If resulting values are within the expected range for N scales we accept that key. */
static bool find_adx_key(STREAMFILE* sf, uint8_t type, uint16_t* xor_start, uint16_t* xor_mult, uint16_t* xor_add, uint16_t subkey) {
    adx_keytest_t keytest = {0};
    bool rc = false;


    /* try to find key in external file first */
    bool keyfile_found = read_external_key(sf, type, xor_start, xor_mult, xor_add, subkey);
    if (keyfile_found)
        return true;


    /* prepare keylist detection */
    bool setup_ok = setup_adx_keytest(&keytest, type, sf);
    if (!setup_ok) goto done;

    if (keytest.bruteframe_count == 0)
        goto done;

    /* try to guess key */
    {
        const adxkey_info* keys = NULL;
        int keycount = 0;

        if (type == 8) {
            keys = adxkey8_list;
            keycount = adxkey8_list_count;
        }
        else { //if (type == 9)
            keys = adxkey9_list;
            keycount = adxkey9_list_count;
        }

#ifdef ADX_BRUTEFORCE
        STREAMFILE* sf_keys = open_streamfile_by_filename(sf, "keys.bin");
        uint8_t* buf = NULL;
        uint64_t keycode = 0;

        if (sf_keys) {
            size_t keys_size = get_streamfile_size(sf_keys);
            buf = malloc(keys_size);
            read_streamfile(buf, 0, keys_size, sf_keys);

            keycount = keys_size - 0x08;
            VGM_LOG("ADX BF: test keys.bin (type %i)\n", 0);
        }
#endif

        /* try all keys until one decrypts correctly vs expected scales */
        for (int key_id = 0; key_id < keycount; key_id++) {
            uint16_t key_xor, key_mul, key_add;

#if defined(ADX_PRINT_KEY_INFO) && !defined(ADX_BRUTEFORCE)
            print_key_info(&keys[key_id], type, subkey);
            continue;
#endif

#ifdef ADX_BRUTEFORCE
            if (buf) {
                keycode = get_u64le(buf + key_id);
                if (keycode == 0)
                    continue;
                cri_key9_derive(keycode, subkey, &key_xor, &key_mul, &key_add);
            }
            else
#endif

            /* get pre-derived XOR values or derive if needed */
            if (keys[key_id].start || keys[key_id].mult || keys[key_id].add) {
                key_xor = keys[key_id].start;
                key_mul = keys[key_id].mult;
                key_add = keys[key_id].add;
            }
            else if (type == 8 && keys[key_id].key8) {
                cri_key8_derive(keys[key_id].key8, &key_xor, &key_mul, &key_add);
            }
            else if (type == 9 && keys[key_id].key9) {
                uint64_t keycode = keys[key_id].key9;
                cri_key9_derive(keycode, subkey, &key_xor, &key_mul, &key_add);
            }
            else {
                VGM_LOG("ADX: incorrectly defined key id=%i\n", key_id);
                continue;
            }

            bool key_ok = validate_adx_key(&keytest, key_xor, key_mul, key_add);
            if (!key_ok)
                continue;

#ifdef ADX_BRUTEFORCE
            VGM_LOG("ADX BF: good key at %x, %08x%08x\n", key_id, (uint32_t)(keycode>>32), (uint32_t)(keycode>>0));
#endif

            *xor_start = key_xor;
            *xor_mult = key_mul;
            *xor_add = key_add;
            rc = true;
            break;
        }


#ifdef ADX_BRUTEFORCE
        close_streamfile(sf_keys);
        free(buf);
#endif
    }

done:
    free(keytest.scales);
    return rc;
}
