#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"
#include "../coding/hca_decoder_clhca.h"

#ifdef VGM_DEBUG_OUTPUT
//#define HCA_BRUTEFORCE
#ifdef HCA_BRUTEFORCE
static void bruteforce_hca_key(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey);
#endif
#endif

static int find_hca_key(hca_codec_data* hca_data, uint64_t* p_keycode, uint16_t subkey);


/* CRI HCA - streamed audio from CRI ADX2/Atom middleware */
VGMSTREAM* init_vgmstream_hca(STREAMFILE* sf) {
    return init_vgmstream_hca_subkey(sf, 0x0000);
}

VGMSTREAM* init_vgmstream_hca_subkey(STREAMFILE* sf, uint16_t subkey) {
    VGMSTREAM* vgmstream = NULL;
    hca_codec_data* hca_data = NULL;
    clHCA_stInfo* hca_info;


    /* checks */
    if ((read_u32be(0x00,sf) & 0x7F7F7F7F) != get_id32be("HCA\0")) /* masked in encrypted files */
        goto fail;

    if (!check_extensions(sf, "hca"))
        return NULL;

    /* init vgmstream and library's context, will validate the HCA */
    hca_data = init_hca(sf);
    if (!hca_data) {
        vgm_logi("HCA: unknown format (report)\n");
        goto fail;
    }

    hca_info = hca_get_info(hca_data);

    /* find decryption key in external file or preloaded list */
    if (hca_info->encryptionEnabled) {
        uint64_t keycode = 0;
        uint8_t keybuf[0x08+0x02];
        size_t keysize;

        keysize = read_key_file(keybuf, 0x08+0x04, sf);
        if (keysize == 0x08) { /* standard */
            keycode = get_u64be(keybuf+0x00);
            if (subkey) {
                keycode = keycode * ( ((uint64_t)subkey << 16u) | ((uint16_t)~subkey + 2u) );
            }
        }
        else if (keysize == 0x08+0x02) { /* seed key + AWB subkey */
            uint64_t file_key = get_u64be(keybuf+0x00);
            uint16_t file_sub = get_u16be(keybuf+0x08);
            keycode = file_key * ( ((uint64_t)file_sub << 16u) | ((uint16_t)~file_sub + 2u) );
        }
#ifdef HCA_BRUTEFORCE
        else if (1) {
            int ok = find_hca_key(hca_data, &keycode, subkey);
            if (!ok)
                bruteforce_hca_key(sf, hca_data, &keycode, subkey);
        }
#endif
        else {
            find_hca_key(hca_data, &keycode, subkey);
        }

        hca_set_encryption_key(hca_data, keycode);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(hca_info->channelCount, hca_info->loopEnabled);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_HCA;
    vgmstream->sample_rate = hca_info->samplingRate;

    vgmstream->num_samples = hca_info->blockCount * hca_info->samplesPerBlock -
            hca_info->encoderDelay - hca_info->encoderPadding;
    vgmstream->loop_start_sample = hca_info->loopStartBlock * hca_info->samplesPerBlock -
            hca_info->encoderDelay + hca_info->loopStartDelay;
    vgmstream->loop_end_sample = hca_info->loopEndBlock * hca_info->samplesPerBlock -
            hca_info->encoderDelay + (hca_info->samplesPerBlock - hca_info->loopEndPadding);
    /* After loop end CRI's encoder removes the rest of the original samples and puts some
     * garbage in the last frame that should be ignored. Optionally it can encode fully preserving
     * the file too, but it isn't detectable, so we'll allow the whole thing just in case */
    //if (vgmstream->loop_end_sample && vgmstream->num_samples > vgmstream->loop_end_sample)
    //    vgmstream->num_samples = vgmstream->loop_end_sample;

    /* this can happen in preloading HCA from memory AWB */
    if (hca_info->blockCount * hca_info->blockSize > get_streamfile_size(sf)) {
        unsigned int max_block = get_streamfile_size(sf) / hca_info->blockSize;
        vgmstream->num_samples = max_block * hca_info->samplesPerBlock -
                hca_info->encoderDelay - hca_info->encoderPadding;
    }

    vgmstream->coding_type = coding_CRI_HCA;
    vgmstream->layout_type = layout_none;
    vgmstream->codec_data = hca_data;

    /* assumed mappings */
    {
        static const uint32_t hca_mappings[] = {
                0,
                mapping_MONO,
                mapping_STEREO,
                mapping_2POINT1,
                mapping_QUAD,
                mapping_5POINT0,
                mapping_5POINT1,
                mapping_7POINT0,
                mapping_7POINT1,
        };

        vgmstream->channel_layout = hca_mappings[vgmstream->channels];
    }

    return vgmstream;

fail:
    free_hca(hca_data);
    return NULL;
}


/* try to find the decryption key from a list */
static int find_hca_key(hca_codec_data* hca_data, uint64_t* p_keycode, uint16_t subkey) {
    const size_t keys_length = sizeof(hcakey_list) / sizeof(hcakey_list[0]);
    int i;
    hca_keytest_t hk = {0};

    hk.best_key = 0xCC55463930DBE1AB; /* defaults to PSO2 key, most common */ 
    hk.subkey = subkey;

    for (i = 0; i < keys_length; i++) {
        hk.key = hcakey_list[i].key;

        test_hca_key(hca_data, &hk);
        if (hk.best_score == 1)
            goto done;

#if 0
        {
            int j;
            size_t subkeys_size = hcakey_list[i].subkeys_size;
            const uint16_t* subkeys = hcakey_list[i].subkeys;
            if (subkeys_size > 0 && subkey == 0) {
                for (j = 0; j < subkeys_size; j++) {
                    hk.subkey = subkeys[j];
                    test_hca_key(hca_data, &hk);
                    if (hk.best_score == 1)
                        goto done;
                }
            }
        }
#endif
    }

done:
    *p_keycode = hk.best_key;
    VGM_ASSERT(hk.best_score > 1, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((*p_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*p_keycode & 0xFFFFFFFF), hk.best_score);
    vgm_asserti(hk.best_score <= 0, "HCA: decryption key not found\n");
    return hk.best_score > 0;
}

/******************* */

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

#ifdef HCA_BRUTEFORCE
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


    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.bin");
    if (!sf_keys) return;

    VGM_LOG("HCA BF: test keys.bin (type %i)\n", type);
    *p_keycode = 0;

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
/*
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32LE_1);
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64BE_1);
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32BE_1);

    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64LE_4);
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32LE_4);
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_64BE_4);
    bruteforce_hca_key_bin_type(sf, hca_data, p_keycode, subkey, HBF_TYPE_32BE_4);
*/
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


    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.txt");
    if (!sf_keys) return;

    VGM_LOG("HCA BF: test keys.txt\n");
    *p_keycode = 0;

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


    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.num");
    if (!sf_keys) return;

    VGM_LOG("HCA BF: test keys.num\n");
    *p_keycode = 0;

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
