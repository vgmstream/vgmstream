#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/maac_vgmstream.h"

/* Decoder raw frames without frame sizes.
 *
 * AAC typically comes in these flavors:
 * - ADTS AAC: frames have a frame header with sync + frame config/sizes (similar to MP3)
 * - MP4 AAC: has a frame size table, then raw frames
 * - ADIF AAC: has a simple header then raw frames, with no frame sizes (very rare)
 * 
 * This decoder handles files similar to ADIF AAC, which aren't normally supported by regular decoders.
 */

// arbitrary src data chunk; decoder can handle partial data (will signal more data to feed)
#define MAX_CHUNK_SIZE 0x800
#define AAC_FRAME_SAMPLES 1024
#define MAX_CHANNELS 8

typedef struct {
    uint8_t* buf;
    uint32_t buf_size;
    uint32_t filled;
} packet_t;

/* opaque struct */
typedef struct {
    packet_t pkt;
    float* fbuf;

    int sample_rate;
    int channels;

    maac_raw aac; //TODO rename to handle? (see examples)
    maac_bitreader br;
    maac_channel *ch;

    int encoder_delay;
    int discard;
} aac_codec_data;


static void free_aac(void* priv_data) {
    aac_codec_data* data = priv_data;
    if (!data) return;

    free(data->ch);

    free(data->pkt.buf);
    free(data->fbuf);
    free(data);
}

static int reset_maac(aac_codec_data* data) {

    maac_raw_init(&data->aac);
    maac_bitreader_init(&data->br);

    // could use maac_raw_config to pass AAC's AudioSpecificConfig data instead
    data->aac.sf_index = maac_sampling_frequency_index(data->sample_rate);

    // info only
    //data->aac.sample_rate = data->sample_rate;
    //data->aac.channel_configuration = maac_channel_config_channels(data->channels);
    //if (data->aac.channel_configuration == 0)
    //    return -1;

    for (int i = 0; i < data->channels; i++) {
        maac_channel_init(&data->ch[i]);
    }

    maac_raw_set_out_channels(&data->aac, data->ch);
    maac_raw_set_num_out_channels(&data->aac, data->channels);

    return 0;
}

void* init_aac_raw(int sample_rate, int channels, int encoder_delay) {
    aac_codec_data* data = NULL;
    int rc;

    if (sample_rate <= 0 || channels <= 0 || channels > MAX_CHANNELS)
        return NULL;

    data = calloc(1, sizeof(aac_codec_data));
    if (!data) goto fail;

    data->ch = calloc(channels, maac_channel_size());
    if (!data->ch) goto fail;

    data->sample_rate = sample_rate;
    data->channels = channels;
    data->encoder_delay = encoder_delay;

    data->pkt.buf_size = MAX_CHUNK_SIZE;
    data->pkt.buf = calloc(data->pkt.buf_size, sizeof(uint8_t));
    if (!data->pkt.buf) goto fail;

    data->fbuf = calloc(AAC_FRAME_SAMPLES * channels, sizeof(float));
    if (!data->fbuf) goto fail;

    rc = reset_maac(data);
    if (rc < 0) goto fail;

    data->discard = encoder_delay;

    return data;
fail:
    free_aac(data);
    return NULL;
}


/* Reads data chunks (more or less than 1 frame) until decodes 1 AAC frame. */
static bool parse_frame(VGMSTREAM* v, packet_t* pkt) {
    aac_codec_data* data = v->codec_data;
    VGMSTREAMCHANNEL* vs = &v->ch[0];

    data->br.data = pkt->buf;

    while (true) {
        MAAC_RESULT res = maac_raw_decode(&data->aac, &data->br);
        if (res == MAAC_CONTINUE) {
            // signals more data needed (doesn't need to be clean frame boundaries)
            data->pkt.filled = read_streamfile(pkt->buf, vs->offset, pkt->buf_size, vs->streamfile);
            if (data->pkt.filled == 0) {
                VGM_LOG("AAC: read error\n");
                return false;
            }

            vs->offset += pkt->filled;

            data->br.pos = 0;
            data->br.len = pkt->filled;

            continue;
        }

        if (res != MAAC_OK) {
            VGM_LOG("AAC: decode error\n");
            return false;
        }

        break;
    }

    return true;
}

static bool decode_frame_aac(VGMSTREAM* v) {
    aac_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;


    bool ok = parse_frame(v, &data->pkt);
    if (!ok)
        return false;

    sbuf_init_f16(&ds->sbuf, data->fbuf, AAC_FRAME_SAMPLES, v->channels);

    float* src_samples[MAX_CHANNELS];
    for (int ch = 0; ch < v->channels; ch++) {
        src_samples[ch] = data->ch[ch].samples;
    }

    ds->sbuf.filled = ds->sbuf.samples;
    sbuf_interleave(&ds->sbuf, src_samples);

    if (data->discard) {
        ds->discard += data->discard;
        data->discard = 0;
    }

    return true;
}

static void reset_aac(void* priv_data) {
    aac_codec_data* data = priv_data;
    if (!data) return;


    int rc = reset_maac(data);
    if (rc < 0)  {
        VGM_LOG("AAC: failed to reset decoder\n");
    }
}

static void seek_aac(VGMSTREAM* v, int32_t num_sample) {
    aac_codec_data* data = v->codec_data;
    if (!data) return;

    reset_aac(data);

    data->discard = num_sample + data->encoder_delay;

    if (v->loop_ch) {
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
    }
}

const codec_info_t aac_decoder = {
    .sample_type = SFMT_F16,
    .decode_frame = decode_frame_aac,
    .free = free_aac,
    .reset = reset_aac,
    .seek = seek_aac,
};
