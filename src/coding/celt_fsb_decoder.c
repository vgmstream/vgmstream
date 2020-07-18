#include "coding.h"

#ifdef VGM_USE_CELT
#include "celt/celt_fsb.h"

#define FSB_CELT_0_06_1_VERSION 0x80000009 /* libcelt-0.6.1 */
#define FSB_CELT_0_11_0_VERSION 0x80000010 /* libcelt-0.11.0 */
#define FSB_CELT_SAMPLES_PER_FRAME 512
#define FSB_CELT_INTERNAL_SAMPLE_RATE 44100
#define FSB_CELT_MAX_DATA_SIZE 0x200 /* from 0x2e~0x172/1d0, all files are CBR though */

/* opaque struct */
struct celt_codec_data {
    sample_t* buffer;

    sample_t* sample_buffer;
    size_t samples_filled; /* number of samples in the buffer */
    size_t samples_used; /* number of samples extracted from the buffer */

    int samples_to_discard;

    int channel_mode;
    celt_lib_t version;
    void *mode_handle;
    void *decoder_handle;
};


/* FSB CELT, frames with custom header and standard data (API info from FMOD DLLs).
 * FMOD used various libcelt versions, thus some tweaks are needed for them to coexist. */

celt_codec_data *init_celt_fsb(int channels, celt_lib_t version) {
    int error = 0, lib_version = 0;
    celt_codec_data* data = NULL;


    data = calloc(1, sizeof(celt_codec_data));
    if (!data) goto fail;

    data->channel_mode = channels; /* should be 1/2, or rejected by libcelt */
    data->version = version;

    switch(data->version) {
        case CELT_0_06_1: /* older FSB4 (FMOD ~4.33) */
            data->mode_handle = celt_0061_mode_create(FSB_CELT_INTERNAL_SAMPLE_RATE, data->channel_mode, FSB_CELT_SAMPLES_PER_FRAME, &error);
            if (!data->mode_handle || error != CELT_OK) goto fail;

            error = celt_0061_mode_info(data->mode_handle, CELT_GET_BITSTREAM_VERSION, &lib_version);
            if (error != CELT_OK || lib_version != FSB_CELT_0_06_1_VERSION) goto fail;

            data->decoder_handle = celt_0061_decoder_create(data->mode_handle);
            if (!data->decoder_handle) goto fail;
            break;

        case CELT_0_11_0: /* newer FSB4 (FMOD ~4.34), FSB5 */
            data->mode_handle = celt_0110_mode_create(FSB_CELT_INTERNAL_SAMPLE_RATE, FSB_CELT_SAMPLES_PER_FRAME, &error); /* "custom" and not ok? */
            if (!data->mode_handle || error != CELT_OK) goto fail;

            error = celt_0110_mode_info(data->mode_handle, CELT_GET_BITSTREAM_VERSION, &lib_version);
            if (error != CELT_OK || lib_version != FSB_CELT_0_11_0_VERSION) goto fail;

            data->decoder_handle = celt_0110_decoder_create_custom(data->mode_handle, data->channel_mode, &error);
            if (!data->decoder_handle || error != CELT_OK) goto fail;
            break;

        default:
            goto fail;
    }

    data->sample_buffer = calloc(sizeof(sample), data->channel_mode * FSB_CELT_SAMPLES_PER_FRAME);
    if (!data->sample_buffer) goto fail;
    /*  there is ~128 samples of encoder delay, but FMOD DLLs don't discard it? */

    return data;

fail:
    free_celt_fsb(data);
    return NULL;
}


void decode_celt_fsb(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels) {
    VGMSTREAMCHANNEL* stream = &vgmstream->ch[0];
    celt_codec_data* data = vgmstream->codec_data;
    int samples_done = 0;


    while (samples_done < samples_to_do) {

        if (data->samples_filled) {  /* consume samples */
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
                       data->sample_buffer + data->samples_used*channels,
                       samples_to_get*channels * sizeof(sample));

                samples_done += samples_to_get;
            }

            /* mark consumed samples */
            data->samples_used += samples_to_get;
            data->samples_filled -= samples_to_get;
        }
        else { /* decode data */
            int status;
            uint8_t data_buffer[FSB_CELT_MAX_DATA_SIZE] = {0};
            size_t bytes, frame_size;


            data->samples_used = 0;

            /* FSB DLLs do seem to check this fixed value */
            if (read_32bitBE(stream->offset+0x00,stream->streamfile) != 0x17C30DF3) {
                goto decode_fail;
            }

            frame_size = read_32bitLE(stream->offset+0x04,stream->streamfile);
            if (frame_size > FSB_CELT_MAX_DATA_SIZE) {
                goto decode_fail;
            }

            /* read and decode one raw block and advance offsets */
            bytes = read_streamfile(data_buffer,stream->offset+0x08, frame_size,stream->streamfile);
            if (bytes != frame_size) goto decode_fail;

            switch(data->version) {
                case CELT_0_06_1:
                    status = celt_0061_decode(data->decoder_handle, data_buffer,bytes, data->sample_buffer);
                    break;

                case CELT_0_11_0:
                    status = celt_0110_decode(data->decoder_handle, data_buffer,bytes, data->sample_buffer, FSB_CELT_SAMPLES_PER_FRAME);
                    break;

                default:
                    goto decode_fail;
            }
            if (status != CELT_OK) goto decode_fail;

            stream->offset += 0x04+0x04+frame_size;
            data->samples_filled += FSB_CELT_SAMPLES_PER_FRAME;
        }
    }

    return;

decode_fail:
    /* on error just put some 0 samples */
    VGM_LOG("CELT: decode fail at %x, missing %i samples\n", (uint32_t)stream->offset, (samples_to_do - samples_done));
    memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample) * channels);
}

void reset_celt_fsb(celt_codec_data* data) {
    if (!data) return;

    /* recreate decoder (mode should not change) */
    switch(data->version) {
        case CELT_0_06_1:
            if (data->decoder_handle) celt_0061_decoder_destroy(data->decoder_handle);

            data->decoder_handle = celt_0061_decoder_create(data->mode_handle);
            if (!data->decoder_handle) goto fail;
            break;

        case CELT_0_11_0:
            if (data->decoder_handle) celt_0110_decoder_destroy(data->decoder_handle);

            data->decoder_handle = celt_0110_decoder_create_custom(data->mode_handle, data->channel_mode, NULL);
            if (!data->decoder_handle) goto fail;
            break;

        default:
            goto fail;
    }

    data->samples_used = 0;
    data->samples_filled = 0;
    data->samples_to_discard = 0;

    return;
fail:
    return; /* decode calls should fail... */
}

void seek_celt_fsb(VGMSTREAM *vgmstream, int32_t num_sample) {
    celt_codec_data* data = vgmstream->codec_data;
    if (!data) return;

    reset_celt_fsb(data);

    data->samples_to_discard = num_sample;

    /* loop offsets are set during decode; force them to stream start so discard works */
    if (vgmstream->loop_ch)
        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
}

void free_celt_fsb(celt_codec_data* data) {
    if (!data) return;

    switch(data->version) {
        case CELT_0_06_1:
            if (data->decoder_handle) celt_0061_decoder_destroy(data->decoder_handle);
            if (data->mode_handle) celt_0061_mode_destroy(data->mode_handle);
            break;

        case CELT_0_11_0:
            if (data->decoder_handle) celt_0110_decoder_destroy(data->decoder_handle);
            if (data->mode_handle) celt_0110_mode_destroy(data->mode_handle);
            break;

        default:
            break;
    }

    free(data->sample_buffer);
    free(data);
}
#endif
