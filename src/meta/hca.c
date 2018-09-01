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
    {
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


#define HCA_KEY_MAX_TEST_CLIPS   400   /* hopefully nobody masters files with more that a handful... */
#define HCA_KEY_MAX_TEST_FRAMES  100   /* ~102400 samples */
#define HCA_KEY_MAX_TEST_SAMPLES 10240 /* ~10 frames of non-blank samples */

/* Tries to find the decryption key from a list. Simply decodes a few frames and checks if there aren't too many
 * clipped samples, as it's common for invalid keys (though possible with valid keys in poorly mastered files). */
static void find_hca_key(hca_codec_data * hca_data, unsigned long long * out_keycode) {
    const size_t keys_length = sizeof(hcakey_list) / sizeof(hcakey_info);
    sample *test_samples = NULL;
    size_t buffer_samples = hca_data->info.samplesPerBlock * hca_data->info.channelCount;
    unsigned long long best_keycode;
    int i;
    int min_clip_count = -1;


    test_samples = malloc(sizeof(sample) * buffer_samples);
    if (!test_samples)
        return; /* ??? */

    best_keycode = 0xCC55463930DBE1AB; /* defaults to PSO2 key, most common */


    /* find a candidate key */
    for (i = 0; i < keys_length; i++) {
        int clip_count = 0, sample_count = 0;
        int frame = 0, s;
        unsigned long long keycode = (unsigned long long)hcakey_list[i].key;

        clHCA_SetKey(hca_data->handle, keycode);
        reset_hca(hca_data);

        /* test enough frames, but not too many */
        while (frame < HCA_KEY_MAX_TEST_FRAMES && frame < hca_data->info.blockCount) {
            decode_hca(hca_data, test_samples, hca_data->info.samplesPerBlock);

            for (s = 0; s < buffer_samples; s++) {
                if (test_samples[s] != 0 && test_samples[s] != -1)
                    sample_count++; /* ignore upper/lower blank samples */

                if (test_samples[s] == 32767 || test_samples[s] == -32768)
                    clip_count++; /* upper/lower clip */
            }

            if (clip_count > HCA_KEY_MAX_TEST_CLIPS)
                break; /* too many, don't bother */
            if (sample_count >= HCA_KEY_MAX_TEST_SAMPLES)
                break; /* enough non-blank samples tested */

            frame++;
        }

        if (min_clip_count < 0 || clip_count < min_clip_count) {
            min_clip_count = clip_count;
            best_keycode = keycode;
        }

        if (min_clip_count == 0)
            break; /* can't get better than this */

        /* a few clips is normal, but some invalid keys may give low numbers too */
        //if (clip_count < 10)
        //    break;
    }

    VGM_ASSERT(min_clip_count > 0,
            "HCA: best key=%08x%08x (clips=%i)\n",
            (uint32_t)((best_keycode >> 32) & 0xFFFFFFFF), (uint32_t)(best_keycode & 0xFFFFFFFF), min_clip_count);

    reset_hca(hca_data);
    *out_keycode = best_keycode;
    free(test_samples);
}
