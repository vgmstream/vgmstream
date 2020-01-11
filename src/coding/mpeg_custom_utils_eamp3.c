#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG

/* parsed info from a single EAMP3 frame */
typedef struct {
    uint32_t extended_flag;
    uint32_t stereo_flag; /* assumed */
    uint32_t unknown_flag; /* unused? */
    uint32_t frame_size; /* full size including headers and pcm block */
    uint32_t pcm_number; /* samples in the PCM block (typically 1 MPEG frame, 1152) */

    uint32_t pre_size; /* size of the header part */
    uint32_t mpeg_size; /* size of the MPEG part */
    uint32_t pcm_size; /* size of the PCM block */
} eamp3_frame_info;

static int eamp3_parse_frame(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, eamp3_frame_info * eaf);
static int eamp3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, eamp3_frame_info * eaf);
static int eamp3_skip_data(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, int at_start);

/* init config and validate */
int mpeg_custom_setup_init_eamp3(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type) {
    mpeg_frame_info info;
    uint16_t frame_header;
    size_t header_size;


    /* test unknown stuff */
    frame_header = (uint16_t)read_16bitLE(start_offset+0x00, streamFile);
    if (frame_header & 0x2000) {
        VGM_LOG("EAMP3: found unknown bit 13\n");
        goto fail;
    }
    if ((frame_header & 0x8000) && (uint32_t)read_32bitLE(start_offset+0x02, streamFile) > 0xFFFF) {
        VGM_LOG("EAMP3: found big PCM block\n");
        goto fail;
    }

    /* get frame info at offset */
    header_size = (frame_header & 0x8000) ? 0x06 : 0x02;
    if (!mpeg_get_frame_info(streamFile, start_offset+header_size, &info))
        goto fail;
    switch(info.layer) {
        case 1: *coding_type = coding_MPEG_layer1; break;
        case 2: *coding_type = coding_MPEG_layer2; break;
        case 3: *coding_type = coding_MPEG_layer3; break;
        default: goto fail;
    }
    data->channels_per_frame = info.channels;
    data->samples_per_frame = info.frame_samples;
    data->bitrate_per_frame = info.bit_rate;
    data->sample_rate_per_frame = info.sample_rate;


    return 1;
fail:
    return 0;
}

/* reads custom frame header + MPEG data + (optional) PCM block */
int mpeg_custom_parse_frame_eamp3(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    eamp3_frame_info eaf;
    int ok;


    if (!eamp3_skip_data(stream, data, num_stream, 1))
        goto fail;

    ok = eamp3_parse_frame(stream, data, &eaf);
    if (!ok) goto fail;

    ms->bytes_in_buffer = read_streamfile(ms->buffer, stream->offset + eaf.pre_size, eaf.mpeg_size, stream->streamfile);

    ok = eamp3_write_pcm_block(stream, data, num_stream, &eaf);
    if (!ok) goto fail;

    stream->offset += eaf.frame_size;

    if (!eamp3_skip_data(stream, data, num_stream, 0))
        goto fail;

    return 1;
fail:
    return 0;
}


static int eamp3_parse_frame(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, eamp3_frame_info * eaf) {
    uint16_t current_header = (uint16_t)read_16bitLE(stream->offset+0x00, stream->streamfile);

    eaf->extended_flag  = (current_header & 0x8000);
    eaf->stereo_flag    = (current_header & 0x4000);
    eaf->unknown_flag   = (current_header & 0x2000);
    eaf->frame_size     = (current_header & 0x1FFF); /* full size including PCM block */
    eaf->pcm_number     = 0;
    if (eaf->extended_flag > 0) {
        eaf->pcm_number = (uint32_t)read_32bitLE(stream->offset+0x02, stream->streamfile);
        eaf->pcm_size   = sizeof(sample) * eaf->pcm_number * data->channels_per_frame;
        eaf->pre_size   = 0x06;
        eaf->mpeg_size  = eaf->frame_size - eaf->pre_size - eaf->pcm_size;
        if (eaf->frame_size < eaf->pre_size + eaf->pcm_size) {
            VGM_LOG("EAMP3: bad pcm size at %x\n", (uint32_t)stream->offset);
            goto fail;
        }
    }
    else {
        eaf->pcm_size   = 0;
        eaf->pre_size   = 0x02;
        eaf->mpeg_size  = eaf->frame_size - eaf->pre_size;
    }

    return 1;
fail:
    return 0;
}

/* write PCM block directly to sample buffer and setup decode discard (see EALayer3). */
static int eamp3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, eamp3_frame_info * eaf) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    size_t bytes_filled;
    int i;


    bytes_filled = sizeof(sample) * ms->samples_filled * data->channels_per_frame;
    if (bytes_filled + eaf->pcm_size > ms->output_buffer_size) {
        VGM_LOG("EAMP3: can't fill the sample buffer with 0x%x\n", eaf->pcm_size);
        goto fail;
    }


    if (eaf->pcm_number) {

        /* read + write PCM block samples (always LE) */
        for (i = 0; i < eaf->pcm_number * data->channels_per_frame; i++) {
            off_t pcm_offset = stream->offset + eaf->pre_size + eaf->mpeg_size + sizeof(sample)*i;
            int16_t pcm_sample = read_16bitLE(pcm_offset,stream->streamfile);
            put_16bitLE(ms->output_buffer + bytes_filled + sizeof(sample)*i, pcm_sample);
        }
        ms->samples_filled += eaf->pcm_number;

        /* modify decoded samples */
        {
            size_t decode_to_discard = eaf->pcm_number; //todo guessed
            ms->decode_to_discard += decode_to_discard;
        }
    }

    return 1;
fail:
    return 0;
}

/* Skip EA-frames from other streams for .sns/sps multichannel (see EALayer3). */
static int eamp3_skip_data(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, int at_start) {
    int ok, i;
    eamp3_frame_info eaf;
    int skips = at_start ? num_stream : data->streams_size - 1 - num_stream;


    for (i = 0; i < skips; i++) {
        ok = eamp3_parse_frame(stream, data, &eaf);
        if (!ok) goto fail;

        //;VGM_LOG("s%i: skipping %x, now at %lx\n", num_stream,eaf.frame_size,stream->offset);
        stream->offset += eaf.frame_size;
    }
    //;VGM_LOG("s%i: skipped %i frames, now at %lx\n", num_stream,skips,stream->offset);

    return 1;
fail:
    return 0;
}

#endif
