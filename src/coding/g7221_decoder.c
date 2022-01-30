#include "coding.h"

#ifdef VGM_USE_G7221
#include "g7221_decoder_lib.h"

#define G7221_MAX_FRAME_SIZE 0x78   /* 960/8 */
#define G7221_MAX_FRAME_SAMPLES 640 /* 32000/50 */

struct g7221_codec_data {
    int channels;
    int frame_size;
    struct g7221_channel_data {
        sample_t buffer[G7221_MAX_FRAME_SAMPLES];
        g7221_handle* handle;
    } *ch;
};

g7221_codec_data* init_g7221(int channels, int frame_size) {
    int i;
    g7221_codec_data* data = NULL;

    if (frame_size > G7221_MAX_FRAME_SIZE)
        goto fail;

    data = calloc(1, sizeof(g7221_codec_data));
    if (!data) goto fail;

    data->channels = channels;
    data->frame_size = frame_size;

    data->ch = calloc(channels, sizeof(struct g7221_channel_data));
    if (!data->ch) goto fail;

    for (i = 0; i < data->channels; i++) {
        data->ch[i].handle = g7221_init(frame_size);
        if (!data->ch[i].handle) goto fail;
    }

    return data;

fail:
    free_g7221(data);
    return NULL;
}


void decode_g7221(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL* ch = &vgmstream->ch[channel];
    g7221_codec_data* data = vgmstream->codec_data;
    struct g7221_channel_data* ch_data = &data->ch[channel];
    int i;

    if (0 == vgmstream->samples_into_block) {
        uint8_t buf[G7221_MAX_FRAME_SIZE];
        size_t bytes;
        size_t read = data->frame_size;

        bytes = read_streamfile(buf, ch->offset, read, ch->streamfile);
        if (bytes != read) {
            //g7221_decode_empty(ch_data->handle, ch_data->buffer);
            memset(ch_data->buffer, 0, sizeof(ch_data->buffer));
            VGM_LOG("S14: EOF read\n");
        }
        else {
            g7221_decode_frame(ch_data->handle, buf, ch_data->buffer);
        }
    }

    for (i = 0; i < samples_to_do; i++) {
        outbuf[i*channelspacing] = ch_data->buffer[vgmstream->samples_into_block+i];
    }
}


void reset_g7221(g7221_codec_data* data) {
    int i;
    if (!data) return;

    for (i = 0; i < data->channels; i++) {
        g7221_reset(data->ch[i].handle);
    }
}

void free_g7221(g7221_codec_data* data) {
    int i;
    if (!data) return;

    for (i = 0; i < data->channels; i++) {
        g7221_free(data->ch[i].handle);
    }
    free(data->ch);
    free(data);
}

void set_key_g7221(g7221_codec_data* data, const uint8_t* key) {
    int i;
    if (!data) return;

    for (i = 0; i < data->channels; i++) {
        g7221_set_key(data->ch[i].handle, key);
    }
}

/* just in case but very few should be enough */
#define S14_KEY_MIN_TEST_FRAMES  3
#define S14_KEY_MAX_TEST_FRAMES  5

/* Test a number of frames to check if current key decrypts correctly.
 * Returns score: <0: error/wrong, 0: unknown/silent, >0: good (closer to 1 is better). */
int test_key_g7221(g7221_codec_data* data, off_t start, STREAMFILE* sf) {
    size_t test_frames = 0, current_frame = 0;
    int total_score = 0;
    int max_frames = (get_streamfile_size(sf) - start) / data->frame_size;
    int cur_ch = 0;

    /* assumes key was set before this call */

    while (test_frames < S14_KEY_MAX_TEST_FRAMES && current_frame < max_frames) {
        int score, res;
        size_t bytes;
        uint8_t buf[G7221_MAX_FRAME_SIZE];


        bytes = read_streamfile(buf, start, data->frame_size, sf);
        if (bytes != data->frame_size) {
            total_score = -1;
            break;
        }

        res = g7221_decode_frame(data->ch[cur_ch].handle, buf, data->ch[cur_ch].buffer);
        if (res < 0) {
            total_score = -1;
            break;
        }

        /* alternate channels (decode may affect scores otherwise) */
        cur_ch = (cur_ch + 1) % data->channels;

        /* good key if it decodes without error, encryption is easily detectable */
        score = 1;

        test_frames++;

        total_score += score;

        start += data->frame_size;
    }

    /* signal best possible score (many perfect frames and few blank frames) */
    if (test_frames > S14_KEY_MIN_TEST_FRAMES && total_score > 0 && total_score <= test_frames) {
        total_score = 1;
    }

    /* clean up decoders */
    reset_g7221(data);

    return total_score;
}

#endif
