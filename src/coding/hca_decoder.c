#include "coding.h"
#include "../base/decode_state.h"
#include "libs/clhca.h"
#include "../base/codec_info.h"


struct hca_codec_data {
    STREAMFILE* sf;
    clHCA_stInfo info;

    void* buf;
    float* fbuf;
    int current_delay;
    unsigned int current_block;

    void* handle;
};

static void reset_hca(void* priv) {
    hca_codec_data* data = priv;

    clHCA_DecodeReset(data->handle);
    data->current_block = 0;
    data->current_delay = data->info.encoderDelay;
}

void free_hca(void* priv) {
    hca_codec_data* data = priv;
    if (!data) return;

    close_streamfile(data->sf);
    clHCA_done(data->handle);
    free(data->handle);
    free(data->buf);
    free(data->fbuf);
    free(data);
}

/* init a HCA stream; STREAMFILE will be duplicated for internal use. */
hca_codec_data* init_hca(STREAMFILE* sf) {
    uint8_t header_buffer[0x1000]; /* hca header buffer data (probable max ~0x400) */
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

    data->buf = malloc(data->info.blockSize);
    if (!data->buf) goto fail;

    data->fbuf = malloc(sizeof(float) * data->info.channelCount * data->info.samplesPerBlock);
    if (!data->fbuf) goto fail;

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

static bool read_packet(VGMSTREAM* v) {
    hca_codec_data* data = v->codec_data;

    // EOF/error
    if (data->current_block >= data->info.blockCount)
        return false;

    // single block of frames
    const unsigned int block_size = data->info.blockSize;
    //VGMSTREAMCHANNEL* vs = &v->ch[0];
    off_t offset = data->info.headerSize + data->current_block * block_size; //vs->offset

    int bytes = read_streamfile(data->buf, offset, block_size, data->sf);
    if (bytes != block_size) {
        VGM_LOG("HCA: read %x vs expected %x bytes at %x\n", bytes, block_size, (uint32_t)offset);
        return false;
    }
    data->current_block++;

    return true;
}

static bool decode_frame_hca(VGMSTREAM* v) {
    bool ok = read_packet(v);
    if (!ok)
        return false;

    decode_state_t* ds = v->decode_state;
    hca_codec_data* data = v->codec_data;
    const unsigned int block_size = data->info.blockSize;


    /* decode frame */
    int status = clHCA_DecodeBlock(data->handle, data->buf, block_size);
    if (status < 0) {
        VGM_LOG("HCA: decode fail, code=%i\n", status);
        return false;
    }

    clHCA_ReadSamples(data->handle, data->fbuf);

    int samples = data->info.samplesPerBlock;
    sbuf_init_flt(&ds->sbuf, data->fbuf, samples, v->channels);
    ds->sbuf.filled = samples;

    if (data->current_delay) {
        ds->discard += data->current_delay;
        data->current_delay = 0;
    }

    return true;
}

static void seek_hca(VGMSTREAM* v, int32_t num_sample) {
    hca_codec_data* data = v->codec_data;
    //decode_state_t* ds = v->decode_state;

    //TODO handle arbitrary seek points to block N

    /* manually calc loop values if not set (should only happen with installed/forced looping,
     * as actual files usually pad encoder delay so earliest loopStartBlock becomes 1-2,
     * probably for decoding cleanup so this may not be as exact) */
    if (data->info.loopStartBlock == 0 && data->info.loopStartDelay == 0) {
        int target_sample = num_sample + data->info.encoderDelay;

        data->info.loopStartBlock = target_sample / data->info.samplesPerBlock;
        data->info.loopStartDelay = target_sample - (data->info.loopStartBlock * data->info.samplesPerBlock);
    }

    data->current_block = data->info.loopStartBlock;
    data->current_delay = data->info.loopStartDelay;
    //ds->discard = data->info.loopStartDelay //overwritten on decode

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
        bytes = read_streamfile(data->buf, offset, block_size, data->sf);
        if (bytes != block_size) {
            /* normally this shouldn't happen, but pre-fetch ACB stop with frames in half, so just keep score */
            //total_score = -1; 
            break;
        }

        score = clHCA_TestBlock(data->handle, data->buf, block_size);

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

const codec_info_t hca_decoder = {
    .sample_type = SFMT_FLT,
    .decode_frame = decode_frame_hca,
    .free = free_hca,
    .reset = reset_hca,
    .seek = seek_hca,
    // frame_samples: 1024 + discard
    // frame_size: variable
};
