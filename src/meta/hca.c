#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"

static void find_hca_key(hca_codec_data * hca_data, unsigned long long * out_keycode);

VGMSTREAM * init_vgmstream_hca(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    hca_codec_data * hca_data = NULL;
    unsigned long long keycode = 0;

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
        uint8_t keybuf[8];
        if (read_key_file(keybuf, 8, streamFile) == 8) {
            keycode = (uint64_t)get_64bitBE(keybuf+0x00);
        } else {
            find_hca_key(hca_data, &keycode);
        }

        clHCA_SetKey(hca_data->handle, keycode); //maybe should be done through hca_decoder.c?
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

    vgmstream->coding_type = coding_CRI_HCA;
    vgmstream->layout_type = layout_none;
    vgmstream->codec_data = hca_data;

    return vgmstream;

fail:
    free_hca(hca_data);
    return NULL;
}


/* Try to find the decryption key from a list. */
static void find_hca_key(hca_codec_data * hca_data, unsigned long long * out_keycode) {
    const size_t keys_length = sizeof(hcakey_list) / sizeof(hcakey_info);
    unsigned long long best_keycode;
    int best_score = -1;
    int i;

    best_keycode = 0xCC55463930DBE1AB; /* defaults to PSO2 key, most common */


    /* find a candidate key */
    for (i = 0; i < keys_length; i++) {
        int score;
        unsigned long long keycode = (unsigned long long)hcakey_list[i].key;

        score = test_hca_key(hca_data, keycode);

        //;VGM_LOG("HCA: test key=%08x%08x, score=%i\n",
        //        (uint32_t)((keycode >> 32) & 0xFFFFFFFF), (uint32_t)(keycode & 0xFFFFFFFF), score);

        /* wrong key */
        if (score < 0)
            continue;

        /* score 0 is not trustable, update too if something better is found */
        if (best_score < 0 || score < best_score || (best_score == 0 && score == 1)) {
            best_score = score;
            best_keycode = keycode;
        }

        /* best possible score */
        if (score == 1) {
            break;
        }
    }

    //;VGM_LOG("HCA: best key=%08x%08x (score=%i)\n",
    //        (uint32_t)((best_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(best_keycode & 0xFFFFFFFF), best_score);

    VGM_ASSERT(best_score > 1, "HCA: best key=%08x%08x (score=%i)\n",
            (uint32_t)((best_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(best_keycode & 0xFFFFFFFF), best_score);
    *out_keycode = best_keycode;
}
