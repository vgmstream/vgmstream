#include "coding.h"
#include <math.h>

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

#define ogl_vorbis_DEFAULT_BUFFER_SIZE 0x8000 /* should be at least the size of the setup header, ~0x2000 */

static void pcm_convert_float_to_16(vorbis_codec_data * data, sample * outbuf, int samples_to_do, float ** pcm);

/**
 * Inits a raw OGL vorbis stream.
 *
 * Vorbis packets are stored in .ogg, which is divided into ogg pages/packets, and the first packets contain necessary
 * vorbis setup. For raw vorbis the setup is stored it elsewhere (i.e.- in the .exe), presumably to shave off some kb
 * per stream and codec startup time. We'll read the external setup and raw data and decode it with libvorbis.
 *
 * OGL simply removes the Ogg layer and uses 16b packet headers, that have the size of the next packet, but
 * the lower 2b need to be removed (usually 00 but 01 for the id packet, not sure about the meaning).
 */
vorbis_codec_data * init_ogl_vorbis_codec_data(STREAMFILE *streamFile, off_t start_offset, off_t * data_start_offset) {
    vorbis_codec_data * data = NULL;

    /* init stuff */
    data = calloc(1,sizeof(vorbis_codec_data));
    if (!data) goto fail;

    data->buffer_size = ogl_vorbis_DEFAULT_BUFFER_SIZE;
    data->buffer = calloc(sizeof(uint8_t), data->buffer_size);
    if (!data->buffer) goto fail;


    /* init vorbis stream state, using 3 fake Ogg setup packets (info, comments, setup/codebooks)
     * libvorbis expects parsed Ogg pages, but we'll fake them with our raw data instead */
    vorbis_info_init(&data->vi);
    vorbis_comment_init(&data->vc);

    data->op.packet = data->buffer;
    data->op.b_o_s = 1; /* fake headers start */

    {
        /* read 3 packets with triad (id/comment/setup), each with an OGL header */
        off_t offset = start_offset;
        size_t packet_size;

        /* normal identificacion packet */
        packet_size = (uint16_t)read_16bitLE(offset, streamFile) >> 2;
        if (packet_size > data->buffer_size) goto fail;
        data->op.bytes = read_streamfile(data->buffer,offset+2,packet_size, streamFile);
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */
        offset += 2+packet_size;

        /* normal comment packet */
        packet_size = (uint16_t)read_16bitLE(offset, streamFile) >> 2;
        if (packet_size > data->buffer_size) goto fail;
        data->op.bytes = read_streamfile(data->buffer,offset+2,packet_size, streamFile);
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */
        offset += 2+packet_size;

        /* normal setup packet */
        packet_size = (uint16_t)read_16bitLE(offset, streamFile) >> 2;
        if (packet_size > data->buffer_size) goto fail;
        data->op.bytes = read_streamfile(data->buffer,offset+2,packet_size, streamFile);
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */
        offset += 2+packet_size;

        /* data starts after triad */
        *data_start_offset = offset;
    }

    data->op.b_o_s = 0; /* end of fake headers */

    /* init vorbis global and block state */
    if (vorbis_synthesis_init(&data->vd,&data->vi) != 0) goto fail;
    if (vorbis_block_init(&data->vd,&data->vb) != 0) goto fail;


    return data;

fail:
    free_ogl_vorbis(data);
    return NULL;
}

/**
 * Decodes raw OGL vorbis
 */
void decode_ogl_vorbis(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels) {
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

            /* this is not actually needed, but feels nicer */
            data->op.granulepos += samples_to_do;
            data->op.packetno++;

            /* get next packet size from the OGL 16b header */
            data->op.bytes = (uint16_t)read_16bitLE(stream->offset, stream->streamfile) >> 2;
            stream->offset += 2;
            if (data->op.bytes == 0 || data->op.bytes == 0xFFFF) {
                goto decode_fail; /* eof or end padding */
            }

            /* read raw block */
            if (read_streamfile(data->buffer,stream->offset, data->op.bytes,stream->streamfile) != data->op.bytes) {
                goto decode_fail; /* wrong packet? */
            }
            stream->offset += data->op.bytes;

            /* parse the fake ogg packet into a logical vorbis block */
            rc = vorbis_synthesis(&data->vb,&data->op);
            if (rc == OV_ENOTAUDIO) {
                VGM_LOG("vorbis_synthesis: not audio packet @ %lx\n",stream->offset); getchar();
                continue; /* not tested */
            } else if (rc != 0) {
                goto decode_fail;
            }

            /* finally decode the logical block into samples */
            rc = vorbis_synthesis_blockin(&data->vd,&data->vb);
            if (rc != 0)  {
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

/* ********************************************** */

void free_ogl_vorbis(vorbis_codec_data * data) {
    if (!data)
        return;

    /* internal decoder cleanp */
    vorbis_info_clear(&data->vi);
    vorbis_comment_clear(&data->vc);
    vorbis_dsp_clear(&data->vd);

    free(data->buffer);
    free(data);
}

void reset_ogl_vorbis(VGMSTREAM *vgmstream) {
    vorbis_codec_data *data = vgmstream->codec_data;

    /* Seeking is provided by the Ogg layer, so with raw vorbis we need seek tables instead.
     * To avoid having to parse different formats we'll just discard until the expected sample */
    vorbis_synthesis_restart(&data->vd);
    data->samples_to_discard = 0;
}

void seek_ogl_vorbis(VGMSTREAM *vgmstream, int32_t num_sample) {
    vorbis_codec_data *data = vgmstream->codec_data;

    /* Seeking is provided by the Ogg layer, so with raw vorbis we need seek tables instead.
     * To avoid having to parse different formats we'll just discard until the expected sample */
    vorbis_synthesis_restart(&data->vd);
    data->samples_to_discard = num_sample;
    if (vgmstream->loop_ch)
        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
}

#endif
