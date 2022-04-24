#include "coding.h"
#include "hca_decoder_clhca.h"


struct hca_codec_data {
    STREAMFILE* sf;
    clHCA_stInfo info;

    signed short* sample_buffer;
    size_t samples_filled;
    size_t samples_consumed;
    size_t samples_to_discard;

    void* data_buffer;

    unsigned int current_block;

    void* handle;
};

/* init a HCA stream; STREAMFILE will be duplicated for internal use. */
hca_codec_data* init_hca(STREAMFILE* sf) {
    uint8_t header_buffer[0x2000]; /* hca header buffer data (probable max ~0x400) */
    hca_codec_data* data = NULL; /* vgmstream HCA context */
    int header_size;
    int status;

    /* test header */
    if (read_streamfile(header_buffer, 0x00, 0x08, sf) != 0x08)
        goto fail;
    header_size = clHCA_isOurFile(header_buffer, 0x08);
    if (header_size < 0 || header_size > 0x1000)
        goto fail;
    if (read_streamfile(header_buffer, 0x00, header_size, sf) != header_size)
        goto fail;

    /* init vgmstream context */
    data = calloc(1, sizeof(hca_codec_data));
    if (!data) goto fail;

    /* init library handle */
    data->handle = calloc(1, clHCA_sizeof());
    clHCA_clear(data->handle);

    status = clHCA_DecodeHeader(data->handle, header_buffer, header_size); /* parse header */
    if (status < 0) {
        VGM_LOG("HCA: unsupported header found, %i\n", status);
        goto fail;
    }

    status = clHCA_getInfo(data->handle, &data->info); /* extract header info */
    if (status < 0) goto fail;

    data->data_buffer = malloc(data->info.blockSize);
    if (!data->data_buffer) goto fail;

    data->sample_buffer = malloc(sizeof(signed short) * data->info.channelCount * data->info.samplesPerBlock);
    if (!data->sample_buffer) goto fail;

    /* load streamfile for reads */
    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    /* set initial values */
    reset_hca(data);

    return data;

fail:
    free_hca(data);
    return NULL;
}

void decode_hca(hca_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {
    int samples_done = 0;
    const unsigned int channels = data->info.channelCount;
    const unsigned int blockSize = data->info.blockSize;


    while (samples_done < samples_to_do) {

        if (data->samples_filled) {
            int samples_to_get = data->samples_filled;

            if (data->samples_to_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_to_discard)
                    samples_to_get = data->samples_to_discard;
                data->samples_to_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do - samples_done)
                    samples_to_get = samples_to_do - samples_done;

                memcpy(outbuf + samples_done*channels,
                       data->sample_buffer + data->samples_consumed*channels,
                       samples_to_get*channels * sizeof(sample));
                samples_done += samples_to_get;
            }

            /* mark consumed samples */
            data->samples_consumed += samples_to_get;
            data->samples_filled -= samples_to_get;
        }
        else {
            off_t offset = data->info.headerSize + data->current_block * blockSize;
            int status;
            size_t bytes;

            /* EOF/error */
            if (data->current_block >= data->info.blockCount) {
                memset(outbuf, 0, (samples_to_do - samples_done) * channels * sizeof(sample));
                break;
            }

            /* read frame */
            bytes = read_streamfile(data->data_buffer, offset, blockSize, data->sf);
            if (bytes != blockSize) {
                VGM_LOG("HCA: read %x vs expected %x bytes at %x\n", bytes, blockSize, (uint32_t)offset);
                break;
            }

            data->current_block++;

            /* decode frame */
            status = clHCA_DecodeBlock(data->handle, (void*)(data->data_buffer), blockSize);
            if (status < 0) {
                VGM_LOG("HCA: decode fail at %x, code=%i\n", (uint32_t)offset, status);
                break;
            }

            /* extract samples */
            clHCA_ReadSamples16(data->handle, data->sample_buffer);

            data->samples_consumed = 0;
            data->samples_filled += data->info.samplesPerBlock;
        }
    }
}

void reset_hca(hca_codec_data* data) {
    if (!data) return;

    clHCA_DecodeReset(data->handle);
    data->current_block = 0;
    data->samples_filled = 0;
    data->samples_consumed = 0;
    data->samples_to_discard = data->info.encoderDelay;
}

void loop_hca(hca_codec_data* data, int32_t num_sample) {
    if (!data) return;

    /* manually calc loop values if not set (should only happen with installed/forced looping,
     * as actual files usually pad encoder delay so earliest loopStartBlock becomes 1-2,
     * probably for decoding cleanup so this may not be as exact) */
    if (data->info.loopStartBlock == 0 && data->info.loopStartDelay == 0) {
        int target_sample = num_sample + data->info.encoderDelay;

        data->info.loopStartBlock = target_sample / data->info.samplesPerBlock;
        data->info.loopStartDelay = target_sample - (data->info.loopStartBlock * data->info.samplesPerBlock);
    }

    data->current_block = data->info.loopStartBlock;
    data->samples_filled = 0;
    data->samples_consumed = 0;
    data->samples_to_discard = data->info.loopStartDelay;
}

void free_hca(hca_codec_data* data) {
    if (!data) return;

    close_streamfile(data->sf);
    clHCA_done(data->handle);
    free(data->handle);
    free(data->data_buffer);
    free(data->sample_buffer);
    free(data);
}

clHCA_stInfo* hca_get_info(hca_codec_data* data) {
    return &data->info;
}

STREAMFILE* hca_get_streamfile(hca_codec_data* data) {
    if (!data) return NULL;
    return data->sf;
}

/* ************************************************************************* */

/* Test a single HCA key and assign an score for comparison. Multiple keys could potentially result
 * in "playable" results (mostly silent with random clips), so it also checks the resulting PCM.
 * Currently wrong keys should be detected during decoding+un-xor test, so this score may not
 * be needed anymore, but keep around in case CRI breaks those tests in the future. */

/* arbitrary scale to simplify score comparisons */
#define HCA_KEY_SCORE_SCALE      10
/* ignores beginning frames (~10 is not uncommon, Dragalia Lost vocal layers have lots) */
#define HCA_KEY_MAX_SKIP_BLANKS  1200
/* 5~15 should be enough, but almost silent or badly mastered files may need tweaks
 * (ex. newer Tales of the Rays files clip a lot) */
#define HCA_KEY_MIN_TEST_FRAMES  3 //7
#define HCA_KEY_MAX_TEST_FRAMES  7 //12
/* score of 10~30 isn't uncommon in a single frame, too many frames over that is unlikely
 * In rare cases of badly mastered frames there are +580. [Iris Mysteria! (Android)]
 * Lesser is preferable (faster skips) but high scores are less common in the current detection. */
//TODO: may need to improve detection by counting silent (0) vs valid samples, as bad keys give lots of 0s
#define HCA_KEY_MAX_FRAME_SCORE  600
#define HCA_KEY_MAX_TOTAL_SCORE  (HCA_KEY_MAX_TEST_FRAMES * 50*HCA_KEY_SCORE_SCALE)

/* Test a number of frames if key decrypts correctly.
 * Returns score: <0: error/wrong, 0: unknown/silent file, >0: good (the closest to 1 the better). */
static int test_hca_score(hca_codec_data* data, hca_keytest_t* hk) {
    size_t test_frames = 0, current_frame = 0, blank_frames = 0;
    int total_score = 0;
    const unsigned int block_size = data->info.blockSize;
    uint32_t offset = hk->start_offset;

    if (!offset)
        offset = data->info.headerSize;

    /* Due to the potentially large number of keys this must be tuned for speed.
     * Buffered IO seems fast enough (not very different reading a large block once vs frame by frame).
     * clHCA_TestBlock could be optimized a bit more. */

    hca_set_encryption_key(data, hk->key, hk->subkey);

    /* Test up to N non-blank frames or until total frames. */
    /* A final score of 0 (=silent) is only possible for short files with all blank frames */

    while (test_frames < HCA_KEY_MAX_TEST_FRAMES && current_frame < data->info.blockCount) {
        int score;
        size_t bytes;

        /* read and test frame */
        bytes = read_streamfile(data->data_buffer, offset, block_size, data->sf);
        if (bytes != block_size) {
            /* normally this shouldn't happen, but pre-fetch ACB stop with frames in half, so just keep score */
            //total_score = -1; 
            break;
        }

        score = clHCA_TestBlock(data->handle, data->data_buffer, block_size);

        /* get first non-blank frame */
        if (!hk->start_offset && score != 0) {
            hk->start_offset = offset;
        }
        offset += bytes;

        if (score < 0 || score > HCA_KEY_MAX_FRAME_SCORE) {
            total_score = -1;
            break;
        }

        current_frame++;

        /* ignore silent frames at the beginning, up to a point (keep skipping as
         * in rare cases there are one non-blank frame then a bunch, that skew results) */
        if (score == 0 && blank_frames < HCA_KEY_MAX_SKIP_BLANKS /*&& !hk->start_offset*/) {
            blank_frames++;
            continue;
        }

        test_frames++;

        /* scale values to make scores of perfect frames more detectable */
        switch(score) {
            case 1:  score = 1; break;
            case 0:  score = 3*HCA_KEY_SCORE_SCALE; break; /* blanks after non-blacks aren't very trustable */
            default: score = score * HCA_KEY_SCORE_SCALE;
        }

        total_score += score;

        /* don't bother checking more frames, other keys will get better scores */
        if (total_score > HCA_KEY_MAX_TOTAL_SCORE)
            break;
    }
    //;VGM_LOG("HCA KEY: blanks=%i, tests=%i, score=%i\n", blank_frames, test_frames, total_score);

    /* signal best possible score (many perfect frames and few blank frames) */
    if (test_frames > HCA_KEY_MIN_TEST_FRAMES && total_score > 0 && total_score <= test_frames) {
        total_score = 1;
    }

    clHCA_DecodeReset(data->handle);
    return total_score;
}

void test_hca_key(hca_codec_data* data, hca_keytest_t* hk) {
    int score;

    score = test_hca_score(data, hk);

    //;VGM_LOG("HCA: test key=%08x%08x, subkey=%04x, score=%i\n",
    //        (uint32_t)((hk->key >> 32) & 0xFFFFFFFF), (uint32_t)(hk->key & 0xFFFFFFFF), hk->subkey, score);

    /* wrong key */
    if (score < 0)
        return;

    /* update if something better is found */
    if (hk->best_score <= 0 || (score < hk->best_score && score > 0)) {
        hk->best_score = score;
        hk->best_key = hk->key; /* base */
    }
}

void hca_set_encryption_key(hca_codec_data* data, uint64_t keycode, uint64_t subkey) {
    if (subkey) {
        keycode = keycode * ( ((uint64_t)subkey << 16u) | ((uint16_t)~subkey + 2u) );
    }
    clHCA_SetKey(data->handle, (unsigned long long)keycode);
}
