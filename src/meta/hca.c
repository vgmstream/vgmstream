#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"
#include "../coding/hca_decoder_clhca.h"

#ifdef VGM_DEBUG_OUTPUT
  //#define HCA_BRUTEFORCE
  #ifdef HCA_BRUTEFORCE
    #include "hca_bf.h"
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

        keysize = read_key_file(keybuf, sizeof(keybuf), sf);
        if (keysize == 0x08) { /* standard */
            keycode = get_u64be(keybuf+0x00);
        }
        else if (keysize == 0x08+0x02) { /* seed key + AWB subkey */
            keycode = get_u64be(keybuf+0x00);
            subkey  = get_u16be(keybuf+0x08);
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

        hca_set_encryption_key(hca_data, keycode, subkey);
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

    /* Assumed mappings; seems correct vs Atom Viewer, that lists L/R/C/LFE/LS/RS and downmixes HCAs like that.
     * USM HCA's seem to be L/R/SL/SR/C/LFE though (probably reordered at USM level, no detection done in Atom Viewer). */
    {
        static const uint32_t hca_mappings[] = {
                0,
                mapping_MONO,
                mapping_STEREO,
                mapping_2POINT1,
                mapping_QUAD_side,
                mapping_5POINT0,
                mapping_5POINT1_surround,
                mapping_7POINT0,
                mapping_7POINT1_surround,
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
