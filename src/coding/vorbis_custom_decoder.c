#include <math.h>
#include "../base/decode_state.h"
#include "../base/sbuf.h"
#include "../base/codec_info.h"
#include "../base/seek_table.h"
#include "coding.h"
#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS

#define VORBIS_CALL_SAMPLES 1024  // allowed frame 'blocksizes' range from 2^6 ~ 2^13 (64 ~ 8192) but we can return partial samples
#define VORBIS_DEFAULT_BUFFER_SIZE 0x8000 // at least the size of the setup header, ~0x2000


void free_vorbis_custom(void* priv_data) {
    vorbis_custom_codec_data* data = priv_data;
    if (!data)
        return;

    /* internal decoder cleanup */
    vorbis_block_clear(&data->vb);
    vorbis_dsp_clear(&data->vd);
    vorbis_comment_clear(&data->vc);
    vorbis_info_clear(&data->vi);

    free(data->buffer);
    free(data->fbuf);
    free(data);
}

/**
 * Inits a vorbis stream of some custom variety.
 *
 * Normally Vorbis packets are stored in .ogg, which is divided into OggS pages/packets, and the first packets contain necessary
 * Vorbis setup. For custom vorbis the OggS layer is replaced/optimized, the setup can be modified or stored elsewhere
 * (i.e.- in the .exe) and raw Vorbis packets may be modified as well, presumably to shave off some kb and/or obfuscate.
 * We'll manually read/modify the data and decode it with libvorbis calls.
 *
 * Reference: https://www.xiph.org/vorbis/doc/libvorbis/overview.html
 */
vorbis_custom_codec_data* init_vorbis_custom(STREAMFILE* sf, off_t start_offset, vorbis_custom_t type, vorbis_custom_config* config) {
    vorbis_custom_codec_data* data = NULL;
    int ok;

    /* init stuff */
    data = calloc(1, sizeof(vorbis_custom_codec_data));
    if (!data) goto fail;

    data->buffer_size = VORBIS_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(data->buffer_size, sizeof(uint8_t));
    if (!data->buffer) goto fail;

    /* keep around to decode too */
    data->type = type;
    memcpy(&data->config, config, sizeof(vorbis_custom_config));


    /* init vorbis stream state, using 3 fake Ogg setup packets (info, comments, setup/codebooks)
     * libvorbis expects parsed Ogg pages, but we'll fake them with our raw data instead */
    vorbis_info_init(&data->vi);
    vorbis_comment_init(&data->vc);

    data->op.packet = data->buffer;
    data->op.b_o_s = 1; /* fake headers start */

    /* init header */
    switch(data->type) {
        case VORBIS_FSB:    ok = vorbis_custom_setup_init_fsb(sf, start_offset, data); break;
        case VORBIS_WWISE:  ok = vorbis_custom_setup_init_wwise(sf, start_offset, data); break;
        case VORBIS_OGL:    ok = vorbis_custom_setup_init_ogl(sf, start_offset, data); break;
        case VORBIS_SK:     ok = vorbis_custom_setup_init_sk(sf, start_offset, data); break;
        case VORBIS_VID1:   ok = vorbis_custom_setup_init_vid1(sf, start_offset, data); break;
        case VORBIS_AWC:    ok = vorbis_custom_setup_init_awc(sf, start_offset, data); break;
        case VORBIS_OOR:    ok = vorbis_custom_setup_init_oor(sf, start_offset, data); break;
        default:            ok = false; break;
    }
    if(!ok)
        goto fail;

    data->op.b_o_s = 0; /* end of fake headers */

    /* init vorbis global and block state */
    if (vorbis_synthesis_init(&data->vd,&data->vi) != 0) goto fail;
    if (vorbis_block_init(&data->vd,&data->vb) != 0) goto fail;


    /* write output */
    config->channels = data->config.channels;
    config->sample_rate = data->config.sample_rate;
    config->last_granule = data->config.last_granule;
    config->data_start_offset = data->config.data_start_offset;

    if (!data->config.stream_end) {
        data->config.stream_end = get_streamfile_size(sf);
    }

    return data;

fail:
    VGM_LOG("VORBIS: init fail at around 0x%x\n", (uint32_t)start_offset);
    free_vorbis_custom(data);
    return NULL;
}

static bool read_packet(VGMSTREAM* v) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    vorbis_custom_codec_data* data = v->codec_data;

    // extra EOF check
    // (may need to drain samples? not a thing in vorbis due to packet types?)
    if (stream->offset >= data->config.stream_end) {
        VGM_LOG("VORBIS: reached stream end %x\n", data->config.stream_end);
        return false;
    }

    // read/transform data into the ogg_packet buffer and advance offsets
    bool ok;
    switch(data->type) {
        case VORBIS_FSB:    ok = vorbis_custom_parse_packet_fsb(stream, data); break;
        case VORBIS_WWISE:  ok = vorbis_custom_parse_packet_wwise(stream, data); break;
        case VORBIS_OGL:    ok = vorbis_custom_parse_packet_ogl(stream, data); break;
        case VORBIS_SK:     ok = vorbis_custom_parse_packet_sk(stream, data); break;
        case VORBIS_VID1:   ok = vorbis_custom_parse_packet_vid1(stream, data); break;
        case VORBIS_AWC:    ok = vorbis_custom_parse_packet_awc(stream, data); break;
        case VORBIS_OOR:    ok = vorbis_custom_parse_packet_oor(stream, data); break;
        default:            ok = false; break;
    }

    return ok;
}

static bool decode_frame(VGMSTREAM* v) {
    decode_state_t* ds = v->decode_state;
    vorbis_custom_codec_data* data = v->codec_data;
    int rc;

    // parse the fake ogg packet into a logical vorbis block
    rc = vorbis_synthesis(&data->vb, &data->op);
    if (rc == OV_ENOTAUDIO) { 
        // rarely happens, seems ok?
        VGM_LOG("VORBIS: not an audio packet (size=0x%x) @ %x\n", (size_t)data->op.bytes, (uint32_t)v->ch[0].offset);
        ds->sbuf.filled = 0;
        return true;
    } 
    else if (rc != 0) {
        //VGM_LOG("VORBIS: synthesis error rc=%i\n", rc);
        return false;
    }

    // finally decode the logical vorbis block into samples
    rc = vorbis_synthesis_blockin(&data->vd,&data->vb);
    if (rc != 0)  {
        //VGM_LOG("VORBIS: block-in error rc=%i\n", rc);
        return false; //?
    }

    data->op.packetno++;

    return true;
}

static int copy_samples(VGMSTREAM* v) {
    decode_state_t* ds = v->decode_state;
    vorbis_custom_codec_data* data = v->codec_data;

    //TODO: helper?
    //TODO: maybe should init in init_vorbis_custom but right now not all vorbises pass channels
    if (data->fbuf == NULL) {
        data->fbuf = malloc(VORBIS_CALL_SAMPLES * sizeof(float) * v->channels);
        if (!data->fbuf) return -1;
    }

    // get PCM samples from libvorbis buffers
    float** pcm = NULL;
    int samples = vorbis_synthesis_pcmout(&data->vd, &pcm);
    if (samples > VORBIS_CALL_SAMPLES)
        samples = VORBIS_CALL_SAMPLES;

    // no more samples in vorbis's buffer
    if (samples == 0)
        return 0;

    // vorbis's planar buffer to interleaved buffer
    sbuf_init_flt(&ds->sbuf, data->fbuf, samples, v->channels);
    ds->sbuf.filled = samples;
    sbuf_interleave(&ds->sbuf, pcm);

    // mark consumed samples from the buffer
    //  (non-consumed samples are returned in next vorbis_synthesis_pcmout calls)
    vorbis_synthesis_read(&data->vd, samples);

    // TODO: useful?
    //data->op.granulepos += samples; // not actually needed

    if (data->current_discard) {
        ds->discard += data->current_discard;
        data->current_discard = 0;
    }

    return samples;
}

static bool decode_frame_vorbis_custom(VGMSTREAM* v) {
    // vorbis may hold samples, return them first
    int ret = copy_samples(v);
    if (ret < 0) return false;
    if (ret > 0) return true;

    // handle new frame
    bool read = read_packet(v);
    if (!read) {
        VGM_LOG("VORBIS: packet read error\n");
        return false;
    }

    // decode current frame
    bool decoded = decode_frame(v);
    if (!decoded) {
        VGM_LOG("VORBIS: packet decode error\n");
        return false;
    }

    // samples will be copied next call
    return true;
}

static void reset_vorbis_custom(void* priv_data) {
    vorbis_custom_codec_data* data = priv_data;
    if (!data) return;

    vorbis_synthesis_restart(&data->vd);
    data->current_discard = 0;

    // OOR/OggS state
    data->current_packet = 0;
    data->packet_count = 0;
    data->flags = 0;
}

static void seek_vorbis_custom(VGMSTREAM* v, int32_t num_sample) {
    vorbis_custom_codec_data* data = v->codec_data;
    if (!data) return;

    /* Seeking is provided by the Ogg layer, so with custom vorbis we need seek tables instead. 
     * Check if seek table was added and use it if possible. */

    /* In Wwise, table seek vs discard seek seems equivalent, save a few files (v35?) at certain offsets,
     * self-corrected after some frames */

    seek_entry_t seek = {0};
    int skip_samples = seek_table_get_entry(v, num_sample, &seek);
    if (skip_samples >= 0) {
        //;VGM_LOG("VORBIS: seek found %i + %i at %x\n", seek.sample, skip_samples, seek.offset);

        // Vorbis removes first packets due to implicit encoder delay; seek table may take it into account
        bool reset = seek_table_get_reset_decoder(v);
        if (seek.sample == 0) //for old Worbis seek tables
            reset = true;
        if (reset)
            reset_vorbis_custom(data);

        data->current_discard = skip_samples;

        v->ch[0].offset = seek.offset;
        if (v->loop_ch)
            v->loop_ch[0].offset = seek.offset;
    }
    else {
        //;VGM_LOG("VORBIS: no seek found\n");

        reset_vorbis_custom(data);
        data->current_discard = num_sample;
        v->ch[0].offset = seek.offset;
        if (v->loop_ch)
            v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
    }

}

static bool seekable_vorbis_custom(VGMSTREAM* v) {
    vorbis_custom_codec_data* data = v->codec_data;
    if (!data) return false;

    switch(data->type) {
        case VORBIS_WWISE:
            return true;
        default:
            return false;
    }
}

int32_t vorbis_custom_get_samples(VGMSTREAM* v) {
    vorbis_custom_codec_data* data = v->codec_data;

    //TODO improve (would need to change a bunch)
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    uint32_t temp = stream->offset;

    // read packets + sum samples (info from revorb: https://yirkha.fud.cz/progs/foobar2000/revorb.cpp)
    int prev_blocksize = 0;
    int32_t samples = 0;
    while (true) {
        bool ok = read_packet(v);
        if (!ok || data->op.bytes == 0) //EOF probably
            break;

        // get blocksize (somewhat similar to samples-per-frame, but must be adjusted)
        int blocksize = vorbis_packet_blocksize(&data->vi, &data->op);
        if (prev_blocksize)
            samples += (prev_blocksize + blocksize) / 4;
        prev_blocksize = blocksize;
    }

    reset_vorbis_custom(data);
    stream->offset = temp;

    return samples;
}

const codec_info_t vorbis_custom_decoder = {
    .sample_type = SFMT_FLT,
    .decode_frame = decode_frame_vorbis_custom,
    .free = free_vorbis_custom,
    .reset = reset_vorbis_custom,
    .seek = seek_vorbis_custom,
    .seekable = seekable_vorbis_custom
};

#endif
