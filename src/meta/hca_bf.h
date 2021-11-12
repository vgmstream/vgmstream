#ifndef _HCA_BF_
#define _HCA_BF_

#include "meta.h"
#include "../coding/coding.h"

#ifdef HCA_BRUTEFORCE

static void bruteforce_process_result(hca_keytest_t* hk, unsigned long long* p_keycode) {
    *p_keycode = hk->best_key;
    if (hk->best_score < 0 || hk->best_score > 10000) {
        VGM_LOG("HCA BF: no good key found\n");
    }
    else {
        VGM_LOG("HCA BF: best key=%08x%08x (score=%i)\n",
                (uint32_t)((*p_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*p_keycode & 0xFFFFFFFF), hk->best_score);
    }
}

typedef enum {
    HBF_TYPE_64LE_1,
    HBF_TYPE_64BE_1,
    HBF_TYPE_32LE_1,
    HBF_TYPE_32BE_1,
    HBF_TYPE_64LE_4,
    HBF_TYPE_64BE_4,
    HBF_TYPE_32LE_4,
    HBF_TYPE_32BE_4,
} HBF_type_t;

/* Bruteforce binary keys in executables and similar files, mainly for some mobile games.
 * Kinda slow but acceptable for ~100MB exes, not very optimized. Unity usually has keys
 * in plaintext (inside levelX or other base files) instead though, use test below. */
static void bruteforce_hca_key_bin_type(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey, HBF_type_t type) {
    STREAMFILE* sf_keys = NULL;
    uint8_t* buf = NULL;
    uint32_t keys_size, bytes;
    int pos, step;
    uint64_t key = 0, old_key = 0;
    hca_keytest_t hk = {0};

    hk.subkey = subkey;
    *p_keycode = 0;

    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.bin");
    if (!sf_keys) return;

    VGM_LOG("HCA BF: test keys.bin (type %i)\n", type);

    keys_size = get_streamfile_size(sf_keys);

    buf = malloc(keys_size);
    if (!buf) {
        VGM_LOG("HCA BF: key file too big!\n");
        goto done;
    }

    bytes = read_streamfile(buf, 0, keys_size, sf_keys);
    if (bytes != keys_size) goto done;

    VGM_LOG("HCA BF: start .bin\n");

    switch(type) {
        case HBF_TYPE_64LE_1:
        case HBF_TYPE_64BE_1:
        case HBF_TYPE_32LE_1:
        case HBF_TYPE_32BE_1: step = 0x01; break;
        case HBF_TYPE_64LE_4:
        case HBF_TYPE_64BE_4:
        case HBF_TYPE_32LE_4:
        case HBF_TYPE_32BE_4: step = 0x04; break;
        default: goto done;
    }

    pos = 0;
    while (pos < keys_size - 8) {
        VGM_ASSERT(pos % 0x1000000 == 0, "HCA: pos %x...\n", pos);

        /* keys are usually u64le but other orders may exist */
        switch(type) {
            case HBF_TYPE_64LE_1: key = get_u64le(buf + pos); break;
            case HBF_TYPE_64BE_1: key = get_u64be(buf + pos); break;
            case HBF_TYPE_32LE_1: key = get_u32le(buf + pos); break;
            case HBF_TYPE_32BE_1: key = get_u32be(buf + pos); break;
            case HBF_TYPE_64LE_4: key = get_u64le(buf + pos); break;
            case HBF_TYPE_64BE_4: key = get_u64be(buf + pos); break;
            case HBF_TYPE_32LE_4: key = get_u32le(buf + pos); break;
            case HBF_TYPE_32BE_4: key = get_u32be(buf + pos); break;
            default: goto done;
        }
        pos += step;

        if (key == 0 || key == old_key)
            continue;
        old_key = key;

        hk.key = key;
        test_hca_key(hca_data, &hk);
        if (hk.best_score == 1)
            goto done;
    }

done:
    bruteforce_process_result(&hk, p_keycode);
    close_streamfile(sf_keys);
    free(buf);
}

static void bruteforce_hca_key_bin(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64LE_1);
    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32LE_1);
    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64BE_1);
    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32BE_1);

    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64LE_4);
    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32LE_4);
    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64BE_4);
    //bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32BE_4);
}


#include <inttypes.h>
//#include <stdio.h>

/* same as the above but for txt lines. */
static void bruteforce_hca_key_txt(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    STREAMFILE* sf_keys = NULL;
    uint8_t* buf = NULL;
    uint32_t keys_size, bytes;
    char line[1024];
    int i = 0, pos;
    uint64_t key = 0;
    hca_keytest_t hk = {0};

    hk.subkey = subkey;
    *p_keycode = 0;

    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.txt");
    if (!sf_keys) return;

    VGM_LOG("HCA BF: test keys.txt\n");

    keys_size = get_streamfile_size(sf_keys);

    buf = malloc(keys_size);
    if (!buf) {
        VGM_LOG("HCA BF: key file too big!\n");
        goto done;
    }

    bytes = read_streamfile(buf, 0, keys_size, sf_keys);
    if (bytes != keys_size) goto done;

    VGM_LOG("HCA BF: start .txt\n");

    pos = 0;
    while (pos < keys_size) {
        int bytes_read, line_ok, count;
        key = 0;

        bytes_read = read_line(line, sizeof(line), pos, sf_keys, &line_ok);
        pos += bytes_read;
        if (!line_ok) continue; /* line too long */

        count = sscanf(line, "%" SCNd64, &key);
        if (count != 1) continue;

        VGM_ASSERT(pos % 10000 == 0, "HCA: count %i...\n", i);

        if (key == 0)
            continue;
        i++;

        hk.key = key;
        test_hca_key(hca_data, &hk);
        if (hk.best_score == 1)
            goto done;
    }

done:
    bruteforce_process_result(&hk, p_keycode);
    close_streamfile(sf_keys);
    free(buf);
}

/* same as the above but good ol' bruteforce numbers (useful for games with keys that are dates) */
static void bruteforce_hca_key_num(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    STREAMFILE* sf_keys = NULL;
    uint32_t keys_size;
    uint64_t min, max;
    uint64_t key = 0;
    hca_keytest_t hk = {0};

    hk.subkey = subkey;
    *p_keycode = 0;

    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.num");
    if (!sf_keys) return;

    VGM_LOG("HCA BF: test keys.num\n");

    keys_size = get_streamfile_size(sf_keys);

    /* don't set too high as it does ~70000 keys per second, do the math */
    if (keys_size < 0x10) {
        min = 0;
        max = 0xFFFFFFFF;
    }
    else {
        min = read_u64be(0x00, sf_keys);
        max = read_u64be(0x08, sf_keys);
    }

    VGM_LOG("HCA BF: start .num\n");

    while (min < max) {
        key = min;

        min++;
        VGM_ASSERT(min % 0x100000 == 0, "HCA: count %x...\n", (uint32_t)min);

        hk.key = key;
        test_hca_key(hca_data, &hk);
        if (hk.best_score == 1)
            goto done;
    }

done:
    bruteforce_process_result(&hk, p_keycode);
    close_streamfile(sf_keys);
}

static void bruteforce_hca_key(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    bruteforce_hca_key_bin(sf, hca_data, p_keycode, subkey);
    if (*p_keycode != 0)
        return;

    bruteforce_hca_key_txt(sf, hca_data, p_keycode, subkey);
    if (*p_keycode != 0)
        return;

    bruteforce_hca_key_num(sf, hca_data, p_keycode, subkey);
    if (*p_keycode != 0)
        return;
}

#endif

#endif /*_HCA_BF_*/
