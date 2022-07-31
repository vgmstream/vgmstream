#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <limits.h>
#include "meta.h"
#include "adx_keys.h"
#include "../coding/coding.h"


#ifdef VGM_DEBUG_OUTPUT
  //#define ADX_BRUTEFORCE
#endif

#define ADX_KEY_MAX_TEST_FRAMES 32768
#define ADX_KEY_TEST_BUFFER_SIZE 0x8000

static int find_adx_key(STREAMFILE* sf, uint8_t type, uint16_t* xor_start, uint16_t* xor_mult, uint16_t* xor_add, uint16_t subkey);

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
    uint8_t encoding_type, frame_size;
    int16_t coef1, coef2;
    uint16_t xor_start = 0, xor_mult = 0, xor_add = 0;

    meta_t header_type;
    coding_t coding_type;


    /* checks*/
    if (read_u16be(0x00,sf) != 0x8000)
        goto fail;

    /* .adx: standard
     * .adp: Headhunter (DC) */
    if (!check_extensions(sf,"adx,adp"))
        goto fail;

    /* CRI checks both 0x8000 and memcmps this */
    start_offset = read_u16be(0x02,sf) + 0x04;
    if (read_u16be(start_offset - 0x06,sf) != 0x2863 ||   /* "(c" */
        read_u32be(start_offset - 0x04,sf) != 0x29435249) /* ")CRI" */
        goto fail;


    encoding_type = read_u8(0x04, sf);
    switch (encoding_type) {
        case 0x02:
            coding_type = coding_CRI_ADX_fixed;
            break;
        case 0x03:
            coding_type = coding_CRI_ADX;
            break;
        case 0x04:
            coding_type = coding_CRI_ADX_exp;
            break;
        default: /* 0x10 is AHX for DC, 0x11 is AHX */
            goto fail;
    }

    /* ADX encoders can't set this value, but is honored by ADXPlay if changed and multiple of 0x12,
     * though output is unusual and may not be fully supported (works in mono so not an interleave)
     * Later versions of the decode just use constant 0x12 ignoring it, though. */
    frame_size = read_u8(0x05, sf);

    if (read_u8(0x06,sf) != 4) /* bits per sample */
        goto fail;

    /* older ADX (adxencd) up to 2ch, newer ADX (criatomencd) up to 8 */
    channels = read_u8(0x07,sf);
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
        if (is_id32be(ainf_offset+0x00,sf, "AINF"))
            ainf_size = read_u32be(ainf_offset+0x04,sf);

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
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;
    vgmstream->meta_type = header_type;


    /* calculate filter coefficients */
    if (coding_type == coding_CRI_ADX_fixed) {
        int i;
        /* standard XA coefs * (2<<11) */
        for (i = 0; i < channels; i++) {
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
        int i;
        float x, y, z, a, b, c;

        x = cutoff;
        y = sample_rate;
        z = cosf(2.0 * M_PI * x / y); /* 2.0 * M_PI: 6.28318548202515f (decomp) */

        a = M_SQRT2 - z;    /* M_SQRT2: 1.41421353816986f (decomp) */
        b = M_SQRT2 - 1.0;  /* M_SQRT2 - 1: 0.414213538169861f (decomp) */
        c = (a - sqrtf((a + b) * (a - b))) / b; /* this seems calculated with a custom algorithm */

        coef1 = (short)(c * 8192);
        coef2 = (short)(c * c * -4096);

        for (i = 0; i < channels; i++) {
            vgmstream->ch[i].adpcm_coef[0] = coef1;
            vgmstream->ch[i].adpcm_coef[1] = coef2;
        }
    }

    /* init decoder */
    {
        int i;

        for (i = 0; i < channels; i++) {
            /* 2 hist shorts per ch, corresponding to the very first original sample repeated (verified with CRI's encoders).
             * Not vital as their effect is small, after a few samples they don't matter, and most songs start in silence. */
            if (hist_offset) {
                vgmstream->ch[i].adpcm_history1_32 = read_s16be(hist_offset + i*4 + 0x00,sf);
                vgmstream->ch[i].adpcm_history2_32 = read_s16be(hist_offset + i*4 + 0x02,sf);
            }

            if (coding_type == coding_CRI_ADX_enc_8 || coding_type == coding_CRI_ADX_enc_9) {
                int j;
                vgmstream->ch[i].adx_channels = channels;
                vgmstream->ch[i].adx_xor = xor_start;
                vgmstream->ch[i].adx_mult = xor_mult;
                vgmstream->ch[i].adx_add = xor_add;

                for (j = 0; j < i; j++)
                    adx_next_key(&vgmstream->ch[i]);
            }
        }
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* ADX key detection works by reading XORed ADPCM scales in frames, and un-XORing with keys in
 * a list. If resulting values are within the expected range for N scales we accept that key. */
static int find_adx_key(STREAMFILE* sf, uint8_t type, uint16_t *xor_start, uint16_t *xor_mult, uint16_t *xor_add, uint16_t subkey) {
    const int frame_size = 0x12;
    uint16_t *scales = NULL;
    uint16_t *prescales = NULL;
    int bruteframe_start = 0, bruteframe_count = -1;
    off_t start_offset;
    int i, rc = 0;


    /* try to find key in external file first */
    {
        uint8_t keybuf[0x40+1] = {0}; /* known max ~0x30, +1 extra null for keystrings */
        size_t key_size;

        /* handle type8 keystrings, key9 keycodes and derived keys too */
        key_size = read_key_file(keybuf, sizeof(keybuf), sf);

        if (key_size > 0) {
            int is_ascii = 0;

            /* keystrings should be ASCII, also needed to tell apart 0x06 strings from derived keys */
            if (type == 8) {
                is_ascii = 1;
                for (i = 0; i < key_size; i++) {
                    if (keybuf[i] < 0x20 || keybuf[i] > 0x7f) {
                        is_ascii = 0;
                        break;
                    }
                }
            }

            if (key_size == 0x06 && !is_ascii) {
                *xor_start = get_u16be(keybuf + 0x00);
                *xor_mult  = get_u16be(keybuf + 0x02);
                *xor_add   = get_u16be(keybuf + 0x04);
                return 1;
            }
            else if (type == 8 && is_ascii) {
                const char* keystring = (const char*)keybuf;
                derive_adx_key8(keystring, xor_start, xor_mult, xor_add);
                return 1;
            }
            else if (type == 9 && key_size == 0x08) {
                uint64_t keycode = get_u64be(keybuf);
                derive_adx_key9(keycode, subkey, xor_start, xor_mult, xor_add);
                return 1;
            }
            else if (type == 9 && key_size == 0x08+0x02) {
                uint64_t file_keycode = get_u64be(keybuf+0x00);
                uint16_t file_subkey  = get_u16be(keybuf+0x08);
                derive_adx_key9(file_keycode, file_subkey, xor_start, xor_mult, xor_add);
                return 1;
            }
        }
        /* no key set or unknown format, try list */
    }

    /* setup totals */
    {
        int frame_count;
        int channels = read_u8(0x07, sf);
        int num_samples = read_s32be(0x0c, sf);
        off_t end_offset;

        start_offset = read_u16be(0x02, sf) + 0x4;
        end_offset = (num_samples + 31) / 32 * frame_size * channels + start_offset; /* samples-to-bytes */

        frame_count = (end_offset - start_offset) / frame_size;
        if (frame_count < bruteframe_count || bruteframe_count < 0)
            bruteframe_count = frame_count;
    }

    /* find longest run of non-zero frames (zero frames aren't good for key testing) */
    {
        static const uint8_t zeroes[0x12] = {0};
        uint8_t frame[0x12];
        int longest_start = -1, longest_count = -1;
        int count = 0;

        for (i = 0; i < bruteframe_count; i++) {
            read_streamfile(frame, start_offset + i*frame_size, frame_size, sf);
            if (memcmp(zeroes, frame, frame_size) != 0)
                count++;
            else
                count = 0;

            /* update new record of non-zero frames */
            if (count > longest_count) {
                longest_count = count;
                longest_start = i - count + 1;
                if (longest_count >= ADX_KEY_MAX_TEST_FRAMES)
                    break;
            }
        }

        /* no non-zero frames */
        if (longest_start == -1) {
            goto done;
        }

        bruteframe_start = longest_start;
        bruteframe_count = longest_count;
        if (bruteframe_count > ADX_KEY_MAX_TEST_FRAMES) //?
            bruteframe_count = ADX_KEY_MAX_TEST_FRAMES;
    }

    /* pre-load scales in a table, to avoid re-reading them per key */
    {
        /* allocate storage for scales */
        scales = malloc(bruteframe_count * sizeof(uint16_t));
        if (!scales) goto done;

        /* prescales are scales before the first test frame, with some blank frames no good
         * for key testing, but we must read to compute XOR value at bruteframe_start */
        if (bruteframe_start > 0) {
            /* allocate storage for prescales */
            prescales = malloc(bruteframe_start * sizeof(uint16_t));
            if (!prescales) goto done;

            /* read the prescales */
            for (i = 0; i < bruteframe_start; i++) {
                prescales[i] = read_16bitBE(start_offset + i*frame_size, sf);
            }
        }

        /* read in the scales */
        for (i = 0; i < bruteframe_count; i++) {
            scales[i] = read_16bitBE(start_offset + (bruteframe_start + i)*frame_size, sf);
        }
    }

    /* try to guess key */
    {
        const adxkey_info *keys = NULL;
        int keycount = 0, keymask = 0;
        int key_id;

        /* setup test mask (used to check high bits that signal un-XORed scale would be too high to be valid) */
        if (type == 8) {
            keys = adxkey8_list;
            keycount = adxkey8_list_count;
            keymask = 0x6000;
        }
        else { //if (type == 9)
            /* smarter XOR as seen in PSO2. The scale is technically 13 bits,
             * but the maximum value assigned by the encoder is 0x1000.
             * This is written to the ADX file as 0xFFF, leaving the high bit
             * empty, which is used to validate a key */
            keys = adxkey9_list;
            keycount = adxkey9_list_count;
            keymask = 0x1000;
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
        for (key_id = 0; key_id < keycount; key_id++) {
            uint16_t key_xor, key_mul, key_add;
            uint16_t xor, mul, add;

#ifdef ADX_BRUTEFORCE
            if (buf) {
                keycode = get_u64be(buf + key_id);
                derive_adx_key9(keycode, subkey, &key_xor, &key_mul, &key_add);
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
                derive_adx_key8(keys[key_id].key8, &key_xor, &key_mul, &key_add);
            }
            else if (type == 9 && keys[key_id].key9) {
                uint64_t keycode = keys[key_id].key9;
                derive_adx_key9(keycode, subkey, &key_xor, &key_mul, &key_add);
            }
            else {
                VGM_LOG("ADX: incorrectly defined key id=%i\n", key_id);
                continue;
            }

            /* temp test values */
            xor = key_xor;
            mul = key_mul;
            add = key_add;

#if 0
            /* derive and print all keys in the list, quick validity test */
            {
                uint16_t test_xor, test_mul, test_add;
                xor = keys[key_id].start;
                mul = keys[key_id].mult;
                add = keys[key_id].add;
                if (type == 8 && keys[key_id].key8) {
                    derive_adx_key8(keys[key_id].key8, &test_xor, &test_mul, &test_add);
                    VGM_LOG("key8: pre=%04x %04x %04x vs calc=%04x %04x %04x = %s (\"%s\")\n",
                            xor,mul,add, test_xor,test_mul,test_add,
                            xor==test_xor && mul==test_mul && add==test_add ? "ok" : "ko", keys[key_id].key8);
                }
                else if (type == 9 && keys[key_id].key9) {
                    derive_adx_key9(keys[key_id].key9, subkey, &test_xor, &test_mul, &test_add);
                    VGM_LOG("key9: pre=%04x %04x %04x vs calc=%04x %04x %04x = %s (%"PRIu64")\n",
                            xor,mul,add, test_xor,test_mul,test_add,
                            xor==test_xor && mul==test_mul && add==test_add ? "ok" : "ko", keys[key_id].key9);
                }
                continue;
            }
#endif

            /* test vs prescales while XOR looks valid */
            for (i = 0; i < bruteframe_start; i++) {
                if ((prescales[i] & keymask) != (xor & keymask) && prescales[i] != 0)
                    break;
                xor = xor * mul + add;
            }
            if (i != bruteframe_start)
                continue;

            /* test vs scales while XOR looks valid */
            for (i = 0; i < bruteframe_count; i++) {
                if ((scales[i] & keymask) != (xor & keymask))
                    break;
                xor = xor * mul + add;
            }
            if (i != bruteframe_count)
                continue;

#ifdef ADX_BRUTEFORCE
            VGM_LOG("ADX BF: good key at %x, %08x%08x\n", key_id, (uint32_t)(keycode>>32), (uint32_t)(keycode>>0));
#endif

            /* all scales are valid, key is good */
            *xor_start = key_xor;
            *xor_mult = key_mul;
            *xor_add = key_add;
            rc = 1;
            break;
        }


#ifdef ADX_BRUTEFORCE
        close_streamfile(sf_keys);
        free(buf);
#endif
    }

done:
    free(scales);
    free(prescales);
    return rc;
}
