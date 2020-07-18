#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG

/**
 * AWC music uses blocks (sfx doesn't), the fun part being each channel has different num_samples/frames
 * per block, so it's unsuitable for the normal "blocked" layout and parsed here instead.
 * Channel data is separate within the block (first all frames of ch0, then ch1, etc), padded, and sometimes
 * the last few frames of a channel are repeated in the new block (marked with the "discard samples" field).
 */

/* block header size, algined/padded to 0x800 */
static size_t get_block_header_size(STREAMFILE *streamFile, off_t offset, mpeg_codec_data *data) {
    size_t header_size = 0;
    int i;
    int entries = data->config.channels;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = data->config.big_endian ? read_32bitBE : read_32bitLE;

    for (i = 0; i < entries; i++) {
        header_size += 0x18;
        header_size += read_32bit(offset + 0x18*i + 0x04, streamFile) * 0x04; /* entries in the table */
    }

    if (header_size % 0x800) /* padded */
        header_size +=  0x800 - (header_size % 0x800);

    return header_size;
}

/* find data that repeats in the beginning of a new block at the end of last block */
static size_t get_repeated_data_size(STREAMFILE *streamFile, off_t new_offset, off_t last_offset) {
    uint8_t new_frame[0x1000];/* buffer to avoid fseek back and forth */
    mpeg_frame_info info;
    off_t off;
    int i;

    /* read block first frame */
    if ( !mpeg_get_frame_info(streamFile, new_offset, &info))
        goto fail;
    if (info.frame_size > 0x1000)
        goto fail;
    if (read_streamfile(new_frame,new_offset, info.frame_size,streamFile) != info.frame_size)
        goto fail;

    /* find the frame in last bytes of prev block */
    off = last_offset - 0x4000; /* typical max is 5-10 frames of ~0x200, no way to know exact size */
    while (off < last_offset) {
        /* compare frame vs prev block data */
        for (i = 0; i < info.frame_size; i++) {
            if ((uint8_t)read_8bit(off+i,streamFile) != new_frame[i])
                break;
        }

        /* frame fully compared? */
        if (i == info.frame_size)
            return last_offset - off;
        else
            off += i+1;
    }

fail:
    VGM_LOG("AWC: can't find repeat size, new=0x%08x, last=0x%08x\n", (uint32_t)new_offset, (uint32_t)last_offset);
    return 0; /* keep on truckin' */
}


/* init config and validate */
int mpeg_custom_setup_init_awc(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type) {
    mpeg_frame_info info;
    int is_music;

    /* start_offset can point to a block header that always starts with 0 (music) or normal data (sfx) */
    is_music = read_32bitBE(start_offset, streamFile) == 0x00000000;
    if (is_music)
        start_offset += get_block_header_size(streamFile, start_offset, data);

    /* get frame info at offset */
    if ( !mpeg_get_frame_info(streamFile, start_offset, &info))
        goto fail;
    switch(info.layer) {
        case 1: *coding_type = coding_MPEG_layer1; break;
        case 2: *coding_type = coding_MPEG_layer2; break;
        case 3: *coding_type = coding_MPEG_layer3; break;
        default: goto fail;
    }
    data->channels_per_frame = info.channels;
    data->samples_per_frame = info.frame_samples;


    /* extra checks */
    if (is_music) {
        if (data->config.chunk_size <= 0)
            goto fail; /* needs block size */
    }

    /* no big encoder delay added (for sfx they can start in less than ~300 samples) */

    return 1;
fail:
    return 0;
}


/* writes data to the buffer and moves offsets, parsing AWC blocks */
int mpeg_custom_parse_frame_awc(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    mpeg_frame_info info;
    size_t current_data_size = 0, data_offset;
    size_t file_size = get_streamfile_size(stream->streamfile);
    int i;


    /* blocked layout used for music */
    if (data->config.chunk_size) {
        off_t last_offset = stream->offset; /* when block end needs to be known */

        /* block ended for this channel, move to next block start */
        if (ms->current_size_count > 0 && ms->current_size_count == ms->current_size_target) {
            //mpg123_open_feed(ms->m); //todo reset maybe needed?

            data_offset = stream->offset - stream->channel_start_offset; /* ignoring header */
            data_offset -= data_offset % data->config.chunk_size; /* start of current block */
            stream->offset = stream->channel_start_offset + data_offset + data->config.chunk_size;

            ms->current_size_count = 0;
            ms->current_size_target = 0;
        }

        /* just in case, shouldn't happen */
        if (stream->offset >= file_size) {
            goto fail;
        }

        /* block starts for this channel, point to mpeg data */
        if (ms->current_size_count == 0) {
            int32_t (*read_32bit)(off_t,STREAMFILE*) = data->config.big_endian ? read_32bitBE : read_32bitLE;
            off_t channel_offset = 0;

            /* block has a header with base info per channel and table per channel (see blocked_awc.c) */
            ms->decode_to_discard   = read_32bit(stream->offset + 0x18*num_stream + 0x08, stream->streamfile);
            ms->current_size_target = read_32bit(stream->offset + 0x18*num_stream + 0x14, stream->streamfile);

            for (i = 0; i < num_stream; i++) { /* num_stream serves as channel */
                size_t channel_size = read_32bit(stream->offset + 0x18*i + 0x14, stream->streamfile);
                if (channel_size % 0x10) /* 32b aligned */
                    channel_size += 0x10 - channel_size % 0x10;

                channel_offset += channel_size;
            }

            //;VGM_ASSERT(ms->decode_to_discard > 0, "AWC: s%i discard of %x found at chunk %lx\n", num_stream, ms->decode_to_discard, stream->offset);
            stream->offset += channel_offset + get_block_header_size(stream->streamfile, stream->offset, data);

            /* A new block may repeat frame bytes from prev block, and decode_to_discard has the number of repeated samples.
             * However in RDR PS3 (not GTA5?) the value can be off (ie. discards 1152 but the repeat decodes to ~1152*4).
             * I can't figure out why, so just find and skip the repeat data manually (probably better for mpg123 too) */
            if (ms->decode_to_discard) {
                size_t repeat = get_repeated_data_size(stream->streamfile, stream->offset, last_offset);
                if (repeat > 0)
                    ms->decode_to_discard = 0;
                stream->offset += repeat;
                ms->current_size_target -= repeat;
            }
        }
    }


    /* update frame */
    if ( !mpeg_get_frame_info(stream->streamfile, stream->offset, &info) )
        goto fail;
    current_data_size = info.frame_size;

    ms->bytes_in_buffer = read_streamfile(ms->buffer,stream->offset, current_data_size, stream->streamfile);

    stream->offset += current_data_size;

    ms->current_size_count += current_data_size;

    return 1;
fail:
    return 0;
}

#endif
