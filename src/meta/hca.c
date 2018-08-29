#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"

static void find_hca_key(hca_codec_data * hca_data, uint8_t * header_buffer, int header_size, unsigned int * out_keycode_upper, unsigned int * out_keycode_lower);

VGMSTREAM * init_vgmstream_hca(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    uint8_t header_buffer[0x8000]; /* hca header buffer data (probably max ~0x400) */

    hca_codec_data * hca_data = NULL; /* vgmstream HCA context */
    unsigned int keycode_upper, keycode_lower;
    int header_size;

    /* checks */
    if ( !check_extensions(streamFile, "hca"))
        return NULL;

    /* init header */
    if (read_streamfile(header_buffer, 0x00, 0x08, streamFile) != 0x08)
        goto fail;
    header_size = clHCA_isOurFile0(header_buffer);
    if (header_size < 0 || header_size > 0x8000) goto fail;

    if (read_streamfile(header_buffer, 0x00, header_size, streamFile) != header_size) goto fail;
    if (clHCA_isOurFile1(header_buffer, header_size) < 0)
        goto fail;

    /* init vgmstream context */
    hca_data = init_hca(streamFile);


    /* find decryption key in external file or preloaded list */
    {
        uint8_t keybuf[8];
        if (read_key_file(keybuf, 8, streamFile) == 8) {
            keycode_upper = get_32bitBE(keybuf+0);
            keycode_lower = get_32bitBE(keybuf+4);
        } else {
            find_hca_key(hca_data, header_buffer, header_size, &keycode_upper, &keycode_lower);
        }
    }

    /* re-init decoder with key (as it must be supplied on header read) */
    clHCA_clear(hca_data->handle, keycode_lower, keycode_upper);
    if (clHCA_Decode(hca_data->handle, header_buffer, header_size, 0) < 0) /* read header at 0x00 */
        goto fail;
    if (clHCA_getInfo(hca_data->handle, &hca_data->info) < 0) /* copy important header values to info struct */
        goto fail;
    reset_hca(hca_data);


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
static void find_hca_key(hca_codec_data * hca_data, uint8_t * header_buffer, int header_size, unsigned int * out_keycode_upper, unsigned int * out_keycode_lower) {
    sample *testbuf = NULL;
    int i, j, bufsize = 0, tempsize;
    size_t keys_length = sizeof(hcakey_list) / sizeof(hcakey_info);

    int min_clip_count = -1;
    /* defaults to PSO2 key, most common */
    unsigned int best_keycode_upper = 0xCC554639;
    unsigned int best_keycode_lower = 0x30DBE1AB;


    /* find a candidate key */
    for (i = 0; i < keys_length; i++) {
        int clip_count = 0, sample_count = 0;
        int frame = 0, s;

        unsigned int keycode_upper, keycode_lower;
        uint64_t key = hcakey_list[i].key;
        keycode_upper = (key >> 32) & 0xFFFFFFFF;
        keycode_lower = (key >>  0) & 0xFFFFFFFF;


        /* re-init HCA with the current key as buffer becomes invalid (probably can be simplified) */
        reset_hca(hca_data);
        if (read_streamfile(header_buffer, 0x00, header_size, hca_data->streamfile) != header_size)
            continue;

        clHCA_clear(hca_data->handle, keycode_lower, keycode_upper);
        if (clHCA_Decode(hca_data->handle, header_buffer, header_size, 0) < 0)
            continue;
        if (clHCA_getInfo(hca_data->handle, &hca_data->info) < 0)
            continue;

        tempsize = sizeof(sample) * clHCA_samplesPerBlock * hca_data->info.channelCount;
        if (tempsize > bufsize) { /* should happen once */
            sample *temp = (sample *)realloc(testbuf, tempsize);
            if (!temp) goto end;
            testbuf = temp;
            bufsize = tempsize;
        }

        /* test enough frames, but not too many */
        while (frame < HCA_KEY_MAX_TEST_FRAMES && frame < hca_data->info.blockCount) {
            j = clHCA_samplesPerBlock;
            decode_hca(hca_data, testbuf, j);

            j *= hca_data->info.channelCount;
            for (s = 0; s < j; s++) {
                if (testbuf[s] != 0x0000 && testbuf[s] != 0xFFFF)
                    sample_count++; /* ignore upper/lower blank samples */

                if (testbuf[s] == 0x7FFF || testbuf[s] == 0x8000)
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
            best_keycode_upper = keycode_upper;
            best_keycode_lower = keycode_lower;
        }

        if (min_clip_count == 0)
            break; /* can't get better than this */

        /* a few clips is normal, but some invalid keys may give low numbers too */
        //if (clip_count < 10)
        //    break;
    }

    /* reset HCA header */
    hca_data->current_block = 0;
    read_streamfile(header_buffer, 0x00, header_size, hca_data->streamfile);

end:
    VGM_ASSERT(min_clip_count > 0, "HCA: best key=%08x%08x (clips=%i)\n", best_keycode_upper,best_keycode_lower, min_clip_count);
    *out_keycode_upper = best_keycode_upper;
    *out_keycode_lower = best_keycode_lower;
    free(testbuf);
}
