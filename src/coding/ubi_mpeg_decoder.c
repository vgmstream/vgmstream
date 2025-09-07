
#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/ubi_mpeg_helpers.h"

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "libs/minimp3.h"

//TODO: needed for smoother segments, but not sure if block samples counts this
// (usually blocks' frames have more samples than defined but not always; maybe should output delay's samples at EOF)
#define UBIMPEG_INITIAL_DISCARD 480 //observed
#define UBIMPEG_SAMPLES_PER_FRAME 1152
#define UBIMPEG_MAX_CHANNELS 2
#define UBIMPEG_INPUT_LIMIT 0x400 //enough for 2 stereo + mono frames


/* opaque struct */
typedef struct {
    bool is_sur2;
    bool is_sur1;

    bitstream_t is;
    mp3dec_t mp3d;
    mp3dec_frame_info_t info;

    uint8_t ibuf[0x2000];   // big enough to limit re-reading
    uint8_t obuf[0x400];   // at least ~300
    uint8_t rbuf[0x400];   // at least 300 * 2
    float fbuf[UBIMPEG_SAMPLES_PER_FRAME * UBIMPEG_MAX_CHANNELS];

    int initial_discard;
} ubimpeg_codec_data;


static void free_ubimpeg(void* priv_data) {
    ubimpeg_codec_data* data = priv_data;
    if (!data) return;

    free(data);
}

void* init_ubimpeg(uint32_t mode) {
    ubimpeg_codec_data* data = NULL;

    data = calloc(1, sizeof(ubimpeg_codec_data));
    if (!data) goto fail;

    // data may start with 'surround mode' flag, otherwise a regular frame with a Ubi-MPEG sync
    if (mode == get_id32be("2RUS")) {
        data->is_sur2 = true;
    }
    else if (mode == get_id32be("1RUS")) {
        data->is_sur1 = true;
        VGM_LOG("UBI-MPEG: 1RUS found\n");
        goto fail;
    }
    else if ((mode >> 20) != 0xFFF) {
        VGM_LOG("UBI-MPEG: unknown format %x\n", mode);
        goto fail;
    }

    data->initial_discard = UBIMPEG_INITIAL_DISCARD;

    bm_setup(&data->is, data->ibuf, 0);
    mp3dec_init(&data->mp3d);

    return data;
fail:
    free_ubimpeg(data);
    return NULL;
}


// Ubi-MPEG is made to keep all data in memory, since frames go one after another.
// Here we ensure that buf is filled enough for the reader, and move data around if needed
// (maybe should make a circular buf bitreader but seems a bit too particular).
static bool setup_input_bitstream(VGMSTREAM* v) {
    ubimpeg_codec_data* data = v->codec_data;
    VGMSTREAMCHANNEL* vs = &v->ch[0];

    //TODO maybe should clamp to block size, but overreads should result in sync error as new blocks start with samples (technically could match)

    // on init should read buf size
    int read_size = sizeof(data->ibuf);
    int pos = 0;

    if (data->is.bufsize > 0) {
        uint32_t ibuf_offset = bm_pos(&data->is) / 8;
        uint32_t ibuf_left = read_size - ibuf_offset;

        // move buf to beginning + setup to fill it fully
        if (ibuf_left < UBIMPEG_INPUT_LIMIT) {
            int ibuf_bitpos = bm_pos(&data->is) % 8;

            memmove(data->ibuf, data->ibuf + ibuf_offset, ibuf_left);
            bm_setup(&data->is, data->ibuf, ibuf_left);
            bm_skip(&data->is, ibuf_bitpos);

            pos = ibuf_left;
            read_size -= ibuf_left;
        }
        else {
            // enough data in buf
            return true;
        }
    }

    int bytes = read_streamfile(data->ibuf + pos, vs->offset, read_size, vs->streamfile);

    bm_fill(&data->is, bytes);
    vs->offset += bytes;

    return true;
}

static int read_frame_ubimpeg(VGMSTREAM* v) {
    ubimpeg_codec_data* data = v->codec_data;

    // prepare and read data for the bitstream, if needed
    setup_input_bitstream(v) ;

    // convert input data into 1 regular mpeg frame
    bitstream_t os = {0};
    bm_setup(&os, data->obuf, sizeof(data->obuf));

    int obuf_size = ubimpeg_transform_frame(&data->is, &os);
    if (!obuf_size) return 0;

    // TODO: handle correctly
    // Ubi-MPEG (MP2) mixes 1 stereo frame + 1 mono frame coefs before synth to (presumably) emulate M/S stereo from MP3.
    // Ignoring the mono frame usually sounds ok enough (as good as 160kbps JS MP2 by 1998 encoders can sound) but
    // sometimes stereo frames only have data for left channel and need the mono frame to complete R.
    if (data->is_sur1 || data->is_sur2) {
        // consume next frame (should be mono) in a separate buffer as otherwise confuses minimp3
        bm_setup(&os, data->rbuf, sizeof(data->rbuf));
        ubimpeg_transform_frame(&data->is, &os);
    }

    return obuf_size;
}

static bool decode_frame_ubimpeg(VGMSTREAM* v) {
    int obuf_size = read_frame_ubimpeg(v);
    if (!obuf_size) {
        return false;
    }

    decode_state_t* ds = v->decode_state;
    ubimpeg_codec_data* data = v->codec_data;

    int samples = mp3dec_decode_frame(&data->mp3d, data->obuf, obuf_size, data->fbuf, &data->info);
    if (samples < 0) {
        return false;
    }

    // TODO: voice .bnm + Ubi-MPEG sets 2 channels but uses mono frames (no xRUS). Possibly the MPEG engine
    // only handle stereo, maybe should dupe L>R. sbuf copying handles this correctly.
    // todo fix
    if (data->info.channels != v->channels) {
        VGM_LOG_ONCE("UBI MPEG: mismatched channels %i vs %i\n", data->info.channels, v->channels);
        //return false;
    }

    sbuf_init_flt(&ds->sbuf, data->fbuf, samples, data->info.channels);
    ds->sbuf.filled = samples;

    if (data->initial_discard) {
        ds->discard += data->initial_discard;
        data->initial_discard = 0;
    }

    return true;
}

static void reset_ubimpeg(void* priv_data) {
    ubimpeg_codec_data* data = priv_data;
    if (!data) return;

    data->initial_discard = UBIMPEG_INITIAL_DISCARD;

    bm_setup(&data->is, data->ibuf, 0);
    mp3dec_init(&data->mp3d);
}

static void seek_ubimpeg(VGMSTREAM* v, int32_t num_sample) {
    ubimpeg_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    if (!data) return;

    reset_ubimpeg(data);

    ds->discard = num_sample;
    if (v->loop_ch) {
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
    }
}

const codec_info_t ubimpeg_decoder = {
    .sample_type = SFMT_FLT,
    .decode_frame = decode_frame_ubimpeg,
    .free = free_ubimpeg,
    .reset = reset_ubimpeg,
    .seek = seek_ubimpeg,
};
