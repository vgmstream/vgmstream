#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG

/* init config and validate per type */
int mpeg_custom_setup_init_default(STREAMFILE* sf, off_t start_offset, mpeg_codec_data* data, coding_t* coding_type) {
    mpeg_frame_info info;


    /* get frame info at offset */
    if (!mpeg_get_frame_info(sf, start_offset, &info))
        goto fail;
    switch(info.layer) {
        case 1: *coding_type = coding_MPEG_layer1; break;
        case 2: *coding_type = coding_MPEG_layer2; break;
        case 3: *coding_type = coding_MPEG_layer3; break;
        default: goto fail;
    }
    data->channels_per_frame = info.channels;
    data->samples_per_frame = info.frame_samples;
    data->bitrate = info.bit_rate;
    data->sample_rate = info.sample_rate;


    /* extra checks per type */
    switch(data->type) {
        case MPEG_XVAG:
            if (data->config.chunk_size <= 0 || data->config.interleave <= 0)
                goto fail; /* needs external fixed size */
            break;

        case MPEG_FSB:
            if (data->config.fsb_padding != 0
                    && data->config.fsb_padding != 2
                    && data->config.fsb_padding != 4
                    && data->config.fsb_padding != 16)
                goto fail; /* aligned to closest 0/2/4/16 bytes */

            /* get find interleave to stream offsets are set up externally */
            {
                int current_data_size = info.frame_size;
                int current_padding = 0;
                /* FSB padding for Layer III or multichannel Layer II */
                if ((info.layer == 3 && data->config.fsb_padding) || data->config.fsb_padding == 16) {
                    current_padding = (current_data_size % data->config.fsb_padding)
                            ? data->config.fsb_padding - (current_data_size % data->config.fsb_padding)
                            : 0;
                }

                data->config.interleave = current_data_size + current_padding; /* should be constant for all stream */
            }
            break;

        //todo simplify/unify XVAG/P3D/SCD/LYN and just feed arbitrary chunks to the decoder
        case MPEG_P3D:
        case MPEG_SCD:
            if (data->config.interleave <= 0)
                goto fail; /* needs external fixed size */
            break;

        case MPEG_LYN:
            if (data->config.interleave <= 0)
                goto fail; /* needs external fixed size */
            data->default_buffer_size = data->config.interleave;
            if (data->default_buffer_size < data->config.interleave_last)
                data->default_buffer_size = data->config.interleave_last;
            break;

        case MPEG_STANDARD:
        case MPEG_AHX:
        case MPEG_EA:
            if (info.channels != data->config.channels)
                goto fail; /* no multichannel expected */
            break;

        default:
            break;  /* nothing special needed */
    }


    //todo: test more: this improves the output, but seems formats aren't usually prepared
    // (and/or the num_samples includes all possible samples in file, so by discarding some it'll reach EOF)

    /* set encoder delay (samples to skip at the beginning of a stream) if needed, which varies with encoder used */
    switch(data->type) {
        //case MPEG_AHX: data->skip_samples = 480; break; /* observed default */
        //case MPEG_P3D: data->skip_samples = info.frame_samples; break; /* matches Radical ADPCM (PC) output */

        /* FSBs (with FMOD DLLs) don't seem to need it. Particularly a few games (all from Wayforward?)
         * contain audible garbage at the beginning, but it's actually there in-game too.
         * Games doing full loops also must not have delay (reuses mpeg state on loop) */
        case MPEG_FSB:
            data->skip_samples = 0; break;

        case MPEG_XVAG: /* set in header and needed for gapless looping */
            data->skip_samples = data->config.skip_samples; break;
        case MPEG_STANDARD:
            data->skip_samples = data->config.skip_samples; break;
        case MPEG_EA:
            /* typical MP2 decoder delay, verified vs sx.exe, also SCHl blocks header takes discard
             * samples into account (so block_samples+240*2+1 = total frame samples) */
            if (info.layer == 2) {
                data->skip_samples = 240*2 + 1;
            }
            /* MP3 probably uses 576 + 528+1 but no known games use it */
            break;
        default:
            break;
    }
    data->samples_to_discard = data->skip_samples;


    return 1;
fail:
    return 0;
}


/* writes data to the buffer and moves offsets */
int mpeg_custom_parse_frame_default(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream) {
    mpeg_custom_stream* ms = &data->streams[num_stream];
    mpeg_frame_info info;
    size_t current_data_size = 0;
    size_t current_padding = 0;
    size_t current_interleave_pre = 0; /* interleaved data size before current stream */
    size_t current_interleave_post = 0; /* interleaved data size after current stream */
    size_t current_interleave = 0; /* interleave in this block (usually this + pre + post = interleave*streams = block) */


    /* Get data size to give to the decoder, per stream. Usually 1 frame at a time,
     * but doesn't really need to be a full frame (decoder would request more data). */
    switch(data->type) {

        case MPEG_XVAG: /* frames of fixed size (though we could read frame info too) */
            current_interleave = data->config.interleave; /* big interleave */
            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_count-1) - current_interleave_pre;

            current_data_size = data->config.chunk_size;
            break;

        case MPEG_FSB: /* frames with padding + interleave */
            current_interleave = data->config.interleave; /* constant for multi-stream FSbs (1 frame + padding) */
            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_count-1) - current_interleave_pre;

            if (stream->offset >= data->config.data_size) {
                VGM_LOG_ONCE("MPEG: fsb overread\n");
                return false;
            }

            if (!mpeg_get_frame_info(stream->streamfile, stream->offset + current_interleave_pre, &info))
                return false;
            current_data_size = info.frame_size;

            /* get FSB padding for Layer III or multichannel Layer II (Layer I isn't supported by FMOD).
             * Padding sometimes contains garbage like the next frame header so we can't feed it to mpg123 or it gets confused. */
            if ((info.layer == 3 && data->config.fsb_padding) || data->config.fsb_padding == 16) {
                current_padding = (current_data_size % data->config.fsb_padding)
                        ? data->config.fsb_padding - (current_data_size % data->config.fsb_padding)
                        : 0;

                /* Rare Mafia II (PS3) bug (GP_0701_music multilang only): some frame paddings "4" are incorrect,
                 * calcs give 0xD0+0x00 but need 0xD0+0x04 (unlike all other fsbs, which never do that).
                 * FMOD tools decode fine, so they may be doing special detection too, since even
                 * re-encoding the same file and using the same FSB flags/modes won't trigger the bug. */
                if (info.layer == 3 && data->config.fsb_padding == 4 && current_data_size == 0xD0) {
                    uint32_t next_header;
                    off_t next_offset;

                    next_offset = stream->offset + current_data_size + current_padding;
                    if (current_interleave && ((next_offset - stream->channel_start_offset + current_interleave_pre + current_interleave_post) % current_interleave == 0)) {
                        next_offset += current_interleave_pre + current_interleave_post;
                    }

                    next_header = read_32bitBE(next_offset, stream->streamfile);
                    if ((next_header & 0xFFE00000) != 0xFFE00000) { /* doesn't land in a proper frame, fix sizes and hope */
                        VGM_LOG_ONCE("MPEG FSB: stream with wrong padding found\n");
                        current_padding = 0x04;
                    }
                }

            }

            VGM_ASSERT(data->streams_count > 1 && current_interleave != current_data_size+current_padding,
                    "MPEG FSB: %i streams with non-constant interleave found @ 0x%08x\n", data->streams_count, (uint32_t)stream->offset);
            break;

        case MPEG_P3D: /* fixed interleave, not frame-aligned (ie. blocks may end/start in part of a frame) */
        case MPEG_SCD:
            current_interleave = data->config.interleave;

            /* check if current interleave block is short */
            {
                off_t block_offset = stream->offset - stream->channel_start_offset;
                size_t next_block = data->streams_count * data->config.interleave;

                if (data->config.data_size && block_offset + next_block >= data->config.data_size)
                    current_interleave = (data->config.data_size % next_block) / data->streams_count; /* short_interleave*/
            }

            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_count-1) - current_interleave_pre;

            current_data_size = current_interleave;
            break;

        case MPEG_LYN:
            /* after N interleaves last block is bigger */
            if (ms->current_size_count < data->config.max_chunks)
                current_interleave = data->config.interleave;
            else if (ms->current_size_count == data->config.max_chunks)
                current_interleave = data->config.interleave_last;
            else
                return false;

            current_interleave_pre  = current_interleave*num_stream;
            current_interleave_post = current_interleave*(data->streams_count-1) - current_interleave_pre;
            //VGM_LOG("o=%lx, %i: %x, %x, %x, %x\n", stream->offset, num_stream, ms->current_size_count, current_interleave, current_interleave_pre, current_interleave_post );

            current_data_size = current_interleave;
            ms->current_size_count++;
            break;

        default: /* standard frames (CBR or VBR) */
            if (!mpeg_get_frame_info(stream->streamfile, stream->offset, &info))
                return false;
            current_data_size = info.frame_size;
            break;
    }
    if (!current_data_size || current_data_size > ms->buffer_size) {
        VGM_LOG("MPEG: incorrect data_size 0x%x vs buffer 0x%x\n", current_data_size, ms->buffer_size);
        return false;
    }

    /* This assumes all streams' offsets start in the first stream, and advances
     * the 'full interleaved block' at once, ex:
     *  start at s0=0x00, s1=0x00, interleave=0x40 (block = 0x40*2=0x80)
     *  @0x00 read 0x40 of s0, skip 0x40 of s1 (block of 0x80 done) > new offset = 0x80
     *  @0x00 skip 0x40 of s0, read 0x40 of s1 (block of 0x80 done) > new offset = 0x800
     */

    /* read chunk (skipping other interleaves if needed) */
    ms->bytes_in_buffer = read_streamfile(ms->buffer,stream->offset + current_interleave_pre, current_data_size, stream->streamfile);


    /* update offsets and skip other streams */
    stream->offset += current_data_size + current_padding;

    /* skip rest of block (interleave per stream) once this stream's interleaved data is done, if defined */
    if (current_interleave && ((stream->offset - stream->channel_start_offset + current_interleave_pre + current_interleave_post) % current_interleave == 0)) {
        stream->offset += current_interleave_pre + current_interleave_post;
    }


    return true;
}
#endif
