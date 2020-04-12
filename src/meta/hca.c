#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"

//#define HCA_BRUTEFORCE
#ifdef HCA_BRUTEFORCE
static void bruteforce_hca_key(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* out_keycode, uint16_t subkey);
#endif
static void find_hca_key(hca_codec_data* hca_data, uint64_t* p_keycode, uint16_t subkey);


/* CRI HCA - streamed audio from CRI ADX2/Atom middleware */
VGMSTREAM * init_vgmstream_hca(STREAMFILE *streamFile) {
    return init_vgmstream_hca_subkey(streamFile, 0x0000);
}

VGMSTREAM * init_vgmstream_hca_subkey(STREAMFILE *streamFile, uint16_t subkey) {
    VGMSTREAM * vgmstream = NULL;
    hca_codec_data * hca_data = NULL;


    /* checks */
    if ( !check_extensions(streamFile, "hca"))
        return NULL;
    if (((uint32_t)read_32bitBE(0x00,streamFile) & 0x7f7f7f7f) != 0x48434100) /* "HCA\0", possibly masked */
        goto fail;

    /* init vgmstream and library's context, will validate the HCA */
    hca_data = init_hca(streamFile);
    if (!hca_data) goto fail;

    /* find decryption key in external file or preloaded list */
    if (hca_data->info.encryptionEnabled) {
        uint64_t keycode = 0;
        uint8_t keybuf[0x08+0x02];
        size_t keysize;

        keysize = read_key_file(keybuf, 0x08+0x04, streamFile);
        if (keysize == 0x08) { /* standard */
            keycode = (uint64_t)get_64bitBE(keybuf+0x00);
            if (subkey) {
                keycode = keycode * ( ((uint64_t)subkey << 16u) | ((uint16_t)~subkey + 2u) );
            }
        }
        else if (keysize == 0x08+0x02) { /* seed key + AWB subkey */
            uint64_t file_key = (uint64_t)get_64bitBE(keybuf+0x00);
            uint16_t file_sub = (uint16_t)get_16bitBE(keybuf+0x08);
            keycode = file_key * ( ((uint64_t)file_sub << 16u) | ((uint16_t)~file_sub + 2u) );
        }
#ifdef HCA_BRUTEFORCE
        else if (1) {
            bruteforce_hca_key(streamFile, hca_data, &keycode, subkey);
        }
#endif
        else {
            find_hca_key(hca_data, &keycode, subkey);
        }

        clHCA_SetKey(hca_data->handle, (unsigned long long)keycode); //maybe should be done through hca_decoder.c?
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(hca_data->info.channelCount, hca_data->info.loopEnabled);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_HCA;
    vgmstream->sample_rate = hca_data->info.samplingRate;

    vgmstream->num_samples = hca_data->info.blockCount * hca_data->info.samplesPerBlock -
            hca_data->info.encoderDelay - hca_data->info.encoderPadding;
    vgmstream->loop_start_sample = hca_data->info.loopStartBlock * hca_data->info.samplesPerBlock -
            hca_data->info.encoderDelay + hca_data->info.loopStartDelay;
    vgmstream->loop_end_sample = hca_data->info.loopEndBlock * hca_data->info.samplesPerBlock -
            hca_data->info.encoderDelay + (hca_data->info.samplesPerBlock - hca_data->info.loopEndPadding);
    /* After loop end CRI's encoder removes the rest of the original samples and puts some
     * garbage in the last frame that should be ignored. Optionally it can encode fully preserving
     * the file too, but it isn't detectable, so we'll allow the whole thing just in case */
    //if (vgmstream->loop_end_sample && vgmstream->num_samples > vgmstream->loop_end_sample)
    //    vgmstream->num_samples = vgmstream->loop_end_sample;

    /* this can happen in preloading HCA from memory AWB */
    if (hca_data->info.blockCount * hca_data->info.blockSize > get_streamfile_size(streamFile)) {
        unsigned int max_block = get_streamfile_size(streamFile) / hca_data->info.blockSize;
        vgmstream->num_samples = max_block * hca_data->info.samplesPerBlock -
                hca_data->info.encoderDelay - hca_data->info.encoderPadding;
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


static inline void test_key(hca_codec_data * hca_data, uint64_t key, uint16_t subkey, int *best_score, uint64_t *best_keycode) {
    int score;

    if (subkey) {
        key = key * ( ((uint64_t)subkey << 16u) | ((uint16_t)~subkey + 2u) );
    }

    score = test_hca_key(hca_data, (unsigned long long)key);

    //;VGM_LOG("HCA: test key=%08x%08x, subkey=%04x, score=%i\n",
    //        (uint32_t)((key >> 32) & 0xFFFFFFFF), (uint32_t)(key & 0xFFFFFFFF), subkey, score);

    /* wrong key */
    if (score < 0)
        return;

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
    int i,j;

    *p_keycode = 0xCC55463930DBE1AB; /* defaults to PSO2 key, most common */

    for (i = 0; i < keys_length; i++) {
        uint64_t key = hcakey_list[i].key;
        size_t subkeys_size = hcakey_list[i].subkeys_size;
        const uint16_t *subkeys = hcakey_list[i].subkeys;

        test_key(hca_data, key, subkey, &best_score, p_keycode);
        if (best_score == 1)
            goto done;

        if (subkeys_size > 0 && subkey == 0) {
            for (j = 0; j < subkeys_size; j++) {
                test_key(hca_data, key, subkeys[j], &best_score, p_keycode);
                if (best_score == 1)
                    goto done;
            }
        }
    }

done:
    VGM_ASSERT(best_score > 1, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((*p_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*p_keycode & 0xFFFFFFFF), best_score);
    VGM_ASSERT(best_score < 0, "HCA: key not found\n");
}

#ifdef HCA_BRUTEFORCE
/* Bruteforce binary keys in executables and similar files, mainly for some mobile games.
 * Kinda slow but acceptable for ~20MB exes, not very optimized. Unity usually has keys
 * in plaintext (inside levelX or other base files) instead though. */
static void bruteforce_hca_key(STREAMFILE* sf, hca_codec_data* hca_data, unsigned long long* out_keycode, uint16_t subkey) {
    STREAMFILE* sf_keys = NULL;
    uint8_t* buf = NULL;
    int best_score = -1;
    off_t keys_size, bytes;
    int i, pos;


    VGM_LOG("HCA: test keys\n");

    *out_keycode = 0;

    /* load whole file in memory for performance (exes with keys shouldn't be too big) */
    sf_keys = open_streamfile_by_filename(sf, "keys.bin");
    if (!sf_keys) goto done;

    keys_size = get_streamfile_size(sf_keys);

    buf = malloc(keys_size);
    if (!buf) goto done;

    bytes = read_streamfile(buf, 0, keys_size, sf_keys);
    if (bytes != keys_size) goto done;

    pos = 0;
    while (pos < keys_size - 4) {
        uint64_t key;

        /* keys are usually u32le lower, u32le upper (u64le) but other orders may exist */
        key = ((uint64_t)get_u32le(buf + pos + 0x00) << 0 ) | ((uint64_t)get_u32le(buf + pos + 0x04) << 32);
      //key = ((uint64_t)get_u32le(buf + pos + 0x00) << 32) | ((uint64_t)get_u32le(buf + pos + 0x04) << 0);
      //key = ((uint64_t)get_u32be(buf + pos + 0x00) << 0 ) | ((uint64_t)get_u32be(buf + pos + 0x04) << 32);
      //key = ((uint64_t)get_u32be(buf + pos + 0x00) << 32) | ((uint64_t)get_u32be(buf + pos + 0x04) << 0);
        if (key == 0)
            continue;

        test_key(hca_data, keys[i], subkey, &best_score, out_keycode);
        if (best_score == 1)
            goto done;

        VGM_ASSERT(pos % 0x100000 == 0, "HCA: pos %x...\n", pos);

        /* observed files have aligned keys in the .text section, change if needed */
        pos += 0x04; //pos++;
    }

done:
    VGM_ASSERT(best_score > 0, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((*out_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(*out_keycode & 0xFFFFFFFF), best_score);
    VGM_ASSERT(best_score < 0, "HCA: key not found\n");
    
    close_streamfile(sf_keys);
    free(buf);
}
#endif
