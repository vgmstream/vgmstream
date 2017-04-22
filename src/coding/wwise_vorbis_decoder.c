#include "coding.h"
#include <math.h>

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

#include "wwise_vorbis_utils.h"


#define WWISE_VORBIS_DEFAULT_BUFFER_SIZE 0x8000 /* should be at least the size of the setup header, ~0x2000 */

static void pcm_convert_float_to_16(vorbis_codec_data * data, sample * outbuf, int samples_to_do, float ** pcm);
static int vorbis_make_header_identification(uint8_t * buf, size_t bufsize, int channels, int sample_rate, int blocksize_short, int blocksize_long);
static int vorbis_make_header_comment(uint8_t * buf, size_t bufsize);

/**
 * Inits a raw Wwise vorbis stream.
 *
 * Vorbis packets are stored in .ogg, which is divided into ogg pages/packets, and the first packets contain necessary
 * vorbis setup. For raw vorbis the setup is stored it elsewhere (i.e.- in the .exe), presumably to shave off some kb
 * per stream and codec startup time. We'll read the external setup and raw data and decode it with libvorbis.
 *
 * Wwise stores a reduced setup, and the raw vorbis packets have mini headers with the block size,
 * and vorbis packets themselves may reduced as well. The format evolved over time so there are some variations.
 * The Wwise implementation uses Tremor (fixed-point Vorbis) but shouldn't matter.
 *
 * Format reverse engineered by hcs in ww2ogg (https://github.com/hcs64/ww2ogg).
 * Info from the official docs (https://www.xiph.org/vorbis/doc/libvorbis/overview.html).
 */
vorbis_codec_data * init_wwise_vorbis_codec_data(STREAMFILE *streamFile, off_t start_offset, int channels, int sample_rate, int blocksize_0_exp, int blocksize_1_exp,
        wwise_setup_type setup_type, wwise_header_type header_type, wwise_packet_type packet_type, int big_endian) {
    vorbis_codec_data * data = NULL;
    size_t header_size, packet_size;

    /* init stuff */
    data = calloc(1,sizeof(vorbis_codec_data));
    if (!data) goto fail;

    data->buffer_size = WWISE_VORBIS_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;

    data->setup_type = setup_type;
    data->header_type = header_type;
    data->packet_type = packet_type;

    /* init vorbis stream state, using 3 fake Ogg setup packets (info, comments, setup/codebooks)
     * libvorbis expects parsed Ogg pages, but we'll fake them with our raw data instead */
    vorbis_info_init(&data->vi);
    vorbis_comment_init(&data->vc);

    data->op.packet = data->buffer;
    data->op.b_o_s = 1; /* fake headers start */

    if (setup_type == HEADER_TRIAD) {
        /* read 3 Wwise packets with triad (id/comment/setup), each with a Wwise header */
        off_t offset = start_offset;

        /* normal identificacion packet */
        header_size = wwise_vorbis_get_header(streamFile, offset, data->header_type, (int*)&data->op.granulepos, &packet_size, big_endian);
        if (!header_size || packet_size > data->buffer_size) goto fail;
        data->op.bytes = read_streamfile(data->buffer,offset+header_size,packet_size, streamFile);
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */
        offset += header_size + packet_size;

        /* normal comment packet */
        header_size = wwise_vorbis_get_header(streamFile, offset, data->header_type, (int*)&data->op.granulepos, &packet_size, big_endian);
        if (!header_size || packet_size > data->buffer_size) goto fail;
        data->op.bytes = read_streamfile(data->buffer,offset+header_size,packet_size, streamFile);
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */
        offset += header_size + packet_size;

        /* normal setup packet */
        header_size = wwise_vorbis_get_header(streamFile, offset, data->header_type, (int*)&data->op.granulepos, &packet_size, big_endian);
        if (!header_size || packet_size > data->buffer_size) goto fail;
        data->op.bytes = read_streamfile(data->buffer,offset+header_size,packet_size, streamFile);
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */
        offset += header_size + packet_size;
    }
    else {
        /* rebuild headers */

        /* new identificacion packet */
        data->op.bytes = vorbis_make_header_identification(data->buffer, data->buffer_size, channels, sample_rate, blocksize_0_exp, blocksize_1_exp);
        if (!data->op.bytes) goto fail;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */

        /* new comment packet */
        data->op.bytes = vorbis_make_header_comment(data->buffer, data->buffer_size);
        if (!data->op.bytes) goto fail;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */

        /* rebuild setup packet */
        data->op.bytes = wwise_vorbis_rebuild_setup(data->buffer, data->buffer_size, streamFile, start_offset, data, big_endian, channels);
        if (!data->op.bytes) goto fail;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */
    }

    data->op.b_o_s = 0; /* end of fake headers */

    /* init vorbis global and block state */
    if (vorbis_synthesis_init(&data->vd,&data->vi) != 0) goto fail;
    if (vorbis_block_init(&data->vd,&data->vb) != 0) goto fail;



    return data;

fail:
    free_wwise_vorbis(data);
    return NULL;
}

/**
 * Decodes raw Wwise Vorbis
 */
void decode_wwise_vorbis(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels) {
    VGMSTREAMCHANNEL *stream = &vgmstream->ch[0];
    vorbis_codec_data * data = vgmstream->codec_data;
    size_t stream_size =  get_streamfile_size(stream->streamfile);
    //data->op.packet = data->buffer;/* implicit from init */
    int samples_done = 0;


    while (samples_done < samples_to_do) {

        /* extra EOF check for edge cases */
        if (stream->offset > stream_size) {
            memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample));
            break;
        }


        if (data->samples_full) {  /* read more samples */
            int samples_to_get;
            float **pcm;

            /* get PCM samples from libvorbis buffers */
            samples_to_get = vorbis_synthesis_pcmout(&data->vd, &pcm);
            if (!samples_to_get) {
                data->samples_full = 0; /* request more if empty*/
                continue;
            }

            if (data->samples_to_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_to_discard)
                    samples_to_get = data->samples_to_discard;
                data->samples_to_discard -= samples_to_get;
            }
            else {
                /* get max samples and convert from Vorbis float pcm to 16bit pcm */
                if (samples_to_get > samples_to_do - samples_done)
                    samples_to_get = samples_to_do - samples_done;
                pcm_convert_float_to_16(data, outbuf + samples_done * channels, samples_to_get, pcm);
                samples_done += samples_to_get;
            }

            /* mark consumed samples from the buffer
             * (non-consumed samples are returned in next vorbis_synthesis_pcmout calls) */
            vorbis_synthesis_read(&data->vd, samples_to_get);
        }
        else { /* read more data */
            int rc;
            size_t header_size, packet_size;

            data->op.packetno++; /* not actually needed, but feels nicer */

            /* reconstruct a Wwise packet, if needed; final bytes may be bigger than packet_size so we get the header offsets here */
            header_size = wwise_vorbis_get_header(stream->streamfile, stream->offset, data->header_type, (int*)&data->op.granulepos, &packet_size, vgmstream->codec_endian);
            if (!header_size || packet_size > data->buffer_size) {
                VGM_LOG("Wwise Vorbis: wrong packet (0x%x) @ %lx\n", packet_size, stream->offset);
                goto decode_fail;
            }

            data->op.bytes = wwise_vorbis_rebuild_packet(data->buffer, data->buffer_size, stream->streamfile,stream->offset, data, vgmstream->codec_endian);
            stream->offset += header_size + packet_size; /* update first to avoid infinite loops */
            if (!data->op.bytes || data->op.bytes >= 0xFFFF) {
                VGM_LOG("Wwise Vorbis: wrong bytes (0x%lx) @ %lx\n", data->op.bytes, stream->offset);
                goto decode_fail;
            }

            /* parse the fake ogg packet into a logical vorbis block */
            rc = vorbis_synthesis(&data->vb,&data->op);
            if (rc == OV_ENOTAUDIO) {
                VGM_LOG("Wwise Vorbis: not audio packet @ %lx\n",stream->offset);
                continue; /* bad packet parsing */
            } else if (rc != 0) {
                VGM_LOG("Wwise Vorbis: cannot parse Vorbis block @ %lx\n",stream->offset);
                goto decode_fail;
            }

            /* finally decode the logical block into samples */
            rc = vorbis_synthesis_blockin(&data->vd,&data->vb);
            if (rc != 0)  {
                VGM_LOG("Wwise Vorbis: cannot decode Vorbis block @ %lx\n",stream->offset);
                goto decode_fail; /* ? */
            }

            data->samples_full = 1;
        }
    }

    return;

decode_fail:
    /* on error just put some 0 samples */
    memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample));
}

/* *************************************************** */

static void pcm_convert_float_to_16(vorbis_codec_data * data, sample * outbuf, int samples_to_do, float ** pcm) {
    /* mostly from Xiph's decoder_example.c */
    int i,j;

    /* convert float PCM (multichannel float array, with pcm[0]=ch0, pcm[1]=ch1, pcm[2]=ch0, etc)
     * to 16 bit signed PCM ints (host order) and interleave + fix clipping */
    for (i = 0; i < data->vi.channels; i++) {
        sample *ptr = outbuf + i;
        float *mono = pcm[i];
        for (j = 0; j < samples_to_do; j++) {
            int val = floor(mono[j] * 32767.f + .5f);
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;

            *ptr = val;
            ptr += data->vi.channels;
        }
    }
}

static int vorbis_make_header_identification(uint8_t * buf, size_t bufsize, int channels, int sample_rate, int blocksize_0_exp, int blocksize_1_exp) {
    int bytes = 0x1e;
    uint8_t blocksizes;

    if (bytes > bufsize) return 0;

    blocksizes = (blocksize_0_exp << 4) | (blocksize_1_exp);

    put_8bit   (buf+0x00, 0x01);            /* packet_type (id) */
    memcpy     (buf+0x01, "vorbis", 6);     /* id */
    put_32bitLE(buf+0x07, 0x00);            /* vorbis_version (fixed) */
    put_8bit   (buf+0x0b, channels);        /* audio_channels */
    put_32bitLE(buf+0x0c, sample_rate);     /* audio_sample_rate */
    put_32bitLE(buf+0x10, 0x00);            /* bitrate_maximum (optional hint) */
    put_32bitLE(buf+0x14, 0x00);            /* bitrate_nominal (optional hint) */
    put_32bitLE(buf+0x18, 0x00);            /* bitrate_minimum (optional hint) */
    put_8bit   (buf+0x1c, blocksizes);      /* blocksize_0 + blocksize_1 nibbles */
    put_8bit   (buf+0x1d, 0x01);            /* framing_flag (fixed) */

    return bytes;
}

static int vorbis_make_header_comment(uint8_t * buf, size_t bufsize) {
    int bytes = 0x19;

    if (bytes > bufsize) return 0;

    put_8bit   (buf+0x00, 0x03);            /* packet_type (comments) */
    memcpy     (buf+0x01, "vorbis", 6);     /* id */
    put_32bitLE(buf+0x07, 0x09);            /* vendor_length */
    memcpy     (buf+0x0b, "vgmstream", 9);  /* vendor_string */
    put_32bitLE(buf+0x14, 0x00);            /* user_comment_list_length */
    put_8bit   (buf+0x18, 0x01);            /* framing_flag (fixed) */

    return bytes;
}

/* *************************************** */

void free_wwise_vorbis(vorbis_codec_data * data) {
    if (!data)
        return;

    /* internal decoder cleanp */
    vorbis_info_clear(&data->vi);
    vorbis_comment_clear(&data->vc);
    vorbis_dsp_clear(&data->vd);

    free(data->buffer);
    free(data);
}

void reset_wwise_vorbis(VGMSTREAM *vgmstream) {
    vorbis_codec_data *data = vgmstream->codec_data;

    /* Seeking is provided by the Ogg layer, so with raw vorbis we need seek tables instead.
     * To avoid having to parse different formats we'll just discard until the expected sample */
    vorbis_synthesis_restart(&data->vd);
    data->samples_to_discard = 0;
}

void seek_wwise_vorbis(VGMSTREAM *vgmstream, int32_t num_sample) {
    vorbis_codec_data *data = vgmstream->codec_data;

    /* Seeking is provided by the Ogg layer, so with raw vorbis we need seek tables instead.
     * To avoid having to parse different formats we'll just discard until the expected sample */
    vorbis_synthesis_restart(&data->vd);
    data->samples_to_discard = num_sample;
    if (vgmstream->loop_ch) /* this func is only using for looping though */
        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
}

#endif
