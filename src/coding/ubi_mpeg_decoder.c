
#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/ubi_mpeg_helpers.h"
#include "libs/minimp3_vgmstream.h"

//TODO: needed for smoother segments, but not sure if block samples counts this
// (usually blocks' frames have more samples than defined but not always; maybe should output delay's samples at EOF)
#define UBIMPEG_ENCODER_DELAY 480 //observed
#define UBIMPEG_SAMPLES_PER_FRAME 1152
#define UBIMPEG_MAX_CHANNELS 2
#define UBIMPEG_INPUT_LIMIT 0x400 //enough for 2 stereo + mono frames


/* opaque struct */
typedef struct {
    int surr_mode;

    bitstream_t is;
    mp3dec_t mp3d_main;
    mp3dec_t mp3d_surr;
    mp3dec_frame_info_t info_main;
    mp3dec_frame_info_t info_surr;

    uint8_t ibuf[0x2000];   // big enough to limit re-reading
    uint8_t obuf_main[0x400];   // at least ~300
    uint8_t obuf_surr[0x400];   // extra buf for surround frames
    int obuf_main_size;
    int obuf_surr_size;
    float fbuf[UBIMPEG_SAMPLES_PER_FRAME * UBIMPEG_MAX_CHANNELS];

    int initial_discard;
} ubimpeg_codec_data;


static void reset_ubimpeg(void* priv_data) {
    ubimpeg_codec_data* data = priv_data;
    if (!data) return;

    data->initial_discard = UBIMPEG_ENCODER_DELAY;

    bm_setup(&data->is, data->ibuf, 0);
    mp3dec_init(&data->mp3d_main);
    if (data->surr_mode == UBIMPEG_SURR_FULL) {
        mp3dec_init(&data->mp3d_surr);
    }
}

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
        data->surr_mode = UBIMPEG_SURR_FULL;
    }
    else if (mode == get_id32be("1RUS")) {
        data->surr_mode = UBIMPEG_SURR_FAKE;
        VGM_LOG("UBI-MPEG: 1RUS found\n");
        goto fail;
    }
    else if ((mode >> 20) != 0xFFF) {
        VGM_LOG("UBI-MPEG: unknown format %x\n", mode);
        goto fail;
    }

    data->initial_discard = UBIMPEG_ENCODER_DELAY;

    reset_ubimpeg(data);

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

static bool read_frame_ubimpeg(VGMSTREAM* v) {
    ubimpeg_codec_data* data = v->codec_data;

    // prepare and read data for the bitstream, if needed
    setup_input_bitstream(v) ;

    // convert input data into 1 regular mpeg frame
    bitstream_t os = {0};

    bm_setup(&os, data->obuf_main, sizeof(data->obuf_main));
    data->obuf_main_size = ubimpeg_transform_frame(&data->is, &os);
    if (!data->obuf_main_size) return false;

    // Ubi-MPEG (MP2) mixes 1 stereo frame + 1 mono frame coefs before synth to (presumably) emulate 'surround' stereo.
    // Ignoring the mono frame usually sounds ok enough (as good as 160kbps JS MP2 by 1998 encoders can sound) but
    // seems to add extra SFX layers or audio details.
    if (data->surr_mode == UBIMPEG_SURR_FULL) {
        // consume next frame (should be mono) in a separate buffer as otherwise confuses minimp3
        bm_setup(&os, data->obuf_surr, sizeof(data->obuf_surr));
        data->obuf_surr_size = ubimpeg_transform_frame(&data->is, &os);
        if (!data->obuf_surr_size) return false;
    }

    return true;
}

static bool decode_frame_ubimpeg(VGMSTREAM* v) {
    bool ok = read_frame_ubimpeg(v);
    if (!ok) {
        return false;
    }

    decode_state_t* ds = v->decode_state;
    ubimpeg_codec_data* data = v->codec_data;

    int samples = mp3dec_decode_frame_ubimpeg(
            &data->mp3d_main, data->obuf_main, data->obuf_main_size, &data->info_main,
            &data->mp3d_surr, data->obuf_surr, data->obuf_surr_size, &data->info_surr,
            data->surr_mode,
            data->fbuf);
    if (samples < 0) {
        VGM_LOG_ONCE("UBI MPEG: error decoding samples");
        return false;
    }

    int output_channels = data->info_main.channels;
    if (data->info_main.channels != v->channels) {
        if (data->info_main.channels == 1 && v->channels == 2 && data->surr_mode == UBIMPEG_SURR_NONE) {
            // voice .bnm + Ubi-MPEG sets 2 channels but uses mono frames (no xRUS). Seemingly their MPEG
            // engine only handles stereo and must dupe L, which internally is done during the synth phase.
            // (without this sbuf will handle it as silent R)
            for (int i = samples - 1; i >= 0; i--) {
                data->fbuf[i * 2 + 1] = data->fbuf[i];
                data->fbuf[i * 2 + 0] = data->fbuf[i];
            }
            output_channels = v->channels;
        }
        else {
            VGM_LOG_ONCE("UBI MPEG: mismatched channels %i vs %i\n", data->info_main.channels, v->channels);
            return false;
        }
    }

    sbuf_init_flt(&ds->sbuf, data->fbuf, samples, output_channels);
    ds->sbuf.filled = samples;

    if (data->initial_discard) {
        ds->discard += data->initial_discard;
        data->initial_discard = 0;
    }

    return true;
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
