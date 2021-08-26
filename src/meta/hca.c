#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"
#include "../coding/hca_decoder_clhca.h"

//#define HCA_BRUTEFORCE
#ifdef HCA_BRUTEFORCE
static void bruteforce_hca_key(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey);
#endif
static void find_hca_key(hca_codec_data* hca_data, uint64_t* p_keycode, uint16_t subkey);


/* CRI HCA - streamed audio from CRI ADX2/Atom middleware */
VGMSTREAM* init_vgmstream_hca(STREAMFILE* sf) {
    return init_vgmstream_hca_subkey(sf, 0x0000);
}

VGMSTREAM* init_vgmstream_hca_subkey(STREAMFILE* sf, uint16_t subkey) {
    VGMSTREAM* vgmstream = NULL;
    hca_codec_data* hca_data = NULL;
    clHCA_stInfo* hca_info;


    /* checks */
    if (!check_extensions(sf, "hca"))
        return NULL;

    if ((read_u32be(0x00,sf) & 0x7F7F7F7F) != 0x48434100) /* "HCA\0", possibly masked */
        goto fail;

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


static inline void test_key(hca_codec_data* hca_data, uint64_t key, uint16_t subkey, int* best_score, uint64_t* best_keycode) {
    int score;

    //;VGM_LOG("HCA: test key=%08x%08x, subkey=%04x\n",
    //        (uint32_t)((key >> 32) & 0xFFFFFFFF), (uint32_t)(key & 0xFFFFFFFF), subkey);

    if (subkey) {
        key = key * ( ((uint64_t)subkey << 16u) | ((uint16_t)~subkey + 2u) );
    }

    score = test_hca_key(hca_data, (unsigned long long)key);

    //;VGM_LOG("HCA: test key=%08x%08x, subkey=%04x, score=%i\n",
    //        (uint32_t)((key >> 32) & 0xFFFFFFFF), (uint32_t)(key & 0xFFFFFFFF), subkey, score);

    /* wrong key */
    if (score < 0)
        return;

    //;VGM_LOG("HCA: ok key=%08x%08x, subkey=%04x, score=%i\n",
    //        (uint32_t)((key >> 32) & 0xFFFFFFFF), (uint32_t)(key & 0xFFFFFFFF), subkey, score);

    /* update if something better is found */
    if (*best_score <= 0 || (score < *best_score && score > 0)) {
        *best_score = score;
        *best_keycode = key;
    }
}

/* try to find the decryption key from a list. */
static void find_hca_key(hca_codec_data* hca_data, uint64_t* p_keycode, uint16_t subkey) {
    const size_t keys_length = sizeof(hcakey_list) / sizeof(hcakey_info);
    int best_score = -1;
    int i;

    *p_keycode = 0xCC55463930DBE1AB; /* defaults to PSO2 key, most common */

    for (i = 0; i < keys_length; i++) {
        uint64_t key = hcakey_list[i].key;

        test_key(hca_data, key, subkey, &best_score, p_keycode);
        if (best_score == 1)
            goto done;

#if 0
        {
            int j;

            size_t subkeys_size = hcakey_list[i].subkeys_size;
            const uint16_t *subkeys = hcakey_list[i].subkeys;
            if (subkeys_size > 0 && subkey == 0) {
                for (j = 0; j < subkeys_size; j++) {
                    test_key(hca_data, key, subkeys[j], &best_score, p_keycode);
                    if (best_score == 1)
                        goto done;
                }
            }
        }
#endif
    }

done:
    VGM_ASSERT(best_score > 1, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((*p_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*p_keycode & 0xFFFFFFFF), best_score);
    vgm_asserti(best_score < 0, "HCA: decryption key not found\n");
}

#ifdef HCA_BRUTEFORCE
/* Bruteforce binary keys in executables and similar files, mainly for some mobile games.
 * Kinda slow but acceptable for ~20MB exes, not very optimized. Unity usually has keys
 * in plaintext (inside levelX or other base files) instead though. */
static void bruteforce_hca_key_bin(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    STREAMFILE* sf_keys = NULL;
    uint8_t* buf = NULL;
    int best_score = 0xFFFFFF, cur_score;
    off_t keys_size, bytes;
    int pos;
    uint64_t old_key = 0;


    VGM_LOG("HCA: test keys.bin\n");

    *p_keycode = 0;

    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.bin");
    if (!sf_keys) goto done;

    keys_size = get_streamfile_size(sf_keys);

    buf = malloc(keys_size);
    if (!buf) goto done;

    bytes = read_streamfile(buf, 0, keys_size, sf_keys);
    if (bytes != keys_size) goto done;

    VGM_LOG("HCA: start\n");

    pos = 0;
    while (pos < keys_size - 4) {
        uint64_t key;
        VGM_ASSERT(pos % 0x1000000 == 0, "HCA: pos %x...\n", pos);

        /* keys are usually u32le lower, u32le upper (u64le) but other orders may exist */
        key = ((uint64_t)get_u32le(buf + pos + 0x00) << 0 ) | ((uint64_t)get_u32le(buf + pos + 0x04) << 32);
      //key = ((uint64_t)get_u32le(buf + pos + 0x00) << 32) | ((uint64_t)get_u32le(buf + pos + 0x04) << 0);
      //key = ((uint64_t)get_u32be(buf + pos + 0x00) << 0 ) | ((uint64_t)get_u32be(buf + pos + 0x04) << 32);
      //key = ((uint64_t)get_u32be(buf + pos + 0x00) << 32) | ((uint64_t)get_u32be(buf + pos + 0x04) << 0);
      //key = ((uint64_t)get_u32le(buf + pos + 0x00) << 0 ) | 0; /* upper bytes not set, ex. P5 */
      //key = ((uint64_t)get_u32be(buf + pos + 0x00) << 0 ) | 0; /* upper bytes not set, ex. P5 */

        /* observed files have aligned keys, change if needed */
        pos += 0x04;
        //pos++;

        if (key == 0 || key == old_key)
            continue;
        old_key = key;

        cur_score = 0;
        test_key(hca_data, key, subkey, &cur_score, p_keycode);
        if (cur_score == 1)
            goto done;

        if (cur_score > 0 && cur_score <= 500) {
            VGM_LOG("HCA: possible key=%08x%08x (score=%i) at %x\n",
                (uint32_t)((key >> 32) & 0xFFFFFFFF), (uint32_t)(key & 0xFFFFFFFF), cur_score, pos-0x04);
            if (best_score > cur_score)
                best_score = cur_score;
        }
    }

done:
    VGM_ASSERT(best_score > 0, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((*p_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*p_keycode & 0xFFFFFFFF), best_score);
    VGM_ASSERT(best_score < 0, "HCA: key not found\n");
    if (best_score < 0 || best_score > 10000)
        *p_keycode = 0;

    close_streamfile(sf_keys);
    free(buf);
}

#include <inttypes.h>
//#include <stdio.h>

/* same as the above but for txt lines. */
static void bruteforce_hca_key_txt(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    STREAMFILE* sf_keys = NULL;
    uint8_t* buf = NULL;
    int best_score = 0xFFFFFF, cur_score;
    off_t keys_size, bytes;
    int i = 0, pos;
    char line[1024];


    VGM_LOG("HCA: test keys.txt\n");

    *p_keycode = 0;

    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.txt");
    if (!sf_keys) goto done;

    keys_size = get_streamfile_size(sf_keys);

    buf = malloc(keys_size);
    if (!buf) goto done;

    bytes = read_streamfile(buf, 0, keys_size, sf_keys);
    if (bytes != keys_size) goto done;

    VGM_LOG("HCA: start\n");

    pos = 0;
    while (pos < keys_size) {
        int bytes_read, line_ok, count;
        uint64_t key = 0;

        bytes_read = read_line(line, sizeof(line), pos, sf_keys, &line_ok);
        if (!line_ok) continue; //???

        pos += bytes_read;

        count = sscanf(line, "%" SCNd64, &key);
        if (count != 1) continue;

        VGM_ASSERT(pos % 100000 == 0, "HCA: count %i...\n", i);

        if (key == 0)
            continue;
        i++;

        cur_score = 0;
        test_key(hca_data, key, subkey, &cur_score, p_keycode);
        if (cur_score == 1)
            goto done;

        if (cur_score > 0 && cur_score <= 500) {
            VGM_LOG("HCA: possible key=%08x%08x (score=%i) at %x\n",
                (uint32_t)((key >> 32) & 0xFFFFFFFF), (uint32_t)(key & 0xFFFFFFFF), cur_score, pos-0x04);
            if (best_score > cur_score)
                best_score = cur_score;
        }
    }

done:
    VGM_LOG("HCA: done %i keys.txt\n", i);
    VGM_ASSERT(best_score > 0, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((*p_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*p_keycode & 0xFFFFFFFF), best_score);
    VGM_ASSERT(best_score < 0, "HCA: key not found\n");
    if (best_score < 0 || best_score > 10000)
        *p_keycode = 0;

    close_streamfile(sf_keys);
    free(buf);
}

static void bruteforce_hca_key(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* p_keycode, uint16_t subkey) {
    bruteforce_hca_key_bin(sf, hca_data, p_keycode, subkey);
    if (*p_keycode != 0)
        return;

    bruteforce_hca_key_txt(sf, hca_data, p_keycode, subkey);
    if (*p_keycode != 0)
        return;
}

#endif
