#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"

#ifdef VGM_USE_FFMPEG

/* internal sizes, can be any value */
#define FFMPEG_DEFAULT_BUFFER_SIZE 2048
#define FFMPEG_DEFAULT_IO_BUFFER_SIZE 128 * 1024

static int init_seek(ffmpeg_codec_data * data);


static volatile int g_ffmpeg_initialized = 0;

/*
 * Global FFmpeg init
 */
static void g_init_ffmpeg()
{
    if (g_ffmpeg_initialized == 1)
    {
        while (g_ffmpeg_initialized < 2);
    }
    else if (g_ffmpeg_initialized == 0)
    {
        g_ffmpeg_initialized = 1;
        av_log_set_flags(AV_LOG_SKIP_REPEATED);
        av_log_set_level(AV_LOG_ERROR);
        av_register_all();
        g_ffmpeg_initialized = 2;
    }
}


/**
 * Generic init FFmpeg and vgmstream for any file supported by FFmpeg.
 * Always called by vgmstream when trying to identify the file type (if the player allows it).
 */
VGMSTREAM * init_vgmstream_ffmpeg(STREAMFILE *streamFile) {
	return init_vgmstream_ffmpeg_offset( streamFile, 0, streamFile->get_size(streamFile) );
}

VGMSTREAM * init_vgmstream_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
    VGMSTREAM *vgmstream = NULL;
    int loop_flag = 0;
    int32_t loop_start = 0, loop_end = 0, num_samples = 0;

    /* init ffmpeg */
    ffmpeg_codec_data *data = init_ffmpeg_offset(streamFile, start, size);
    if (!data) return NULL;


    /* try to get .pos data */
    {
        uint8_t posbuf[4+4+4];

        if ( read_pos_file(posbuf, 4+4+4, streamFile) ) {
            loop_start = get_32bitLE(posbuf+0);
            loop_end = get_32bitLE(posbuf+4);
            loop_flag = 1; /* incorrect looping will be validated outside */
            /* FFmpeg can't always determine totalSamples correctly so optionally load it (can be 0/NULL)
             * won't crash and will output silence if no loop points and bigger than actual stream's samples */
            num_samples = get_32bitLE(posbuf+8);
        }
    }


    /* build VGMSTREAM */
    vgmstream = allocate_vgmstream(data->channels, loop_flag);
    if (!vgmstream) goto fail;
    
    vgmstream->loop_flag = loop_flag;
    vgmstream->codec_data = data;
    vgmstream->channels = data->channels;
    vgmstream->sample_rate = data->sampleRate;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_FFmpeg;

    if (!num_samples) {
        num_samples = data->totalSamples;
    }
    vgmstream->num_samples = num_samples;

    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        vgmstream->loop_end_sample = loop_end;
    }

    /* this may happen for some streams if FFmpeg can't determine it */
    if (vgmstream->num_samples <= 0)
        goto fail;


    return vgmstream;
    
fail:
    free_ffmpeg(data);
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    
    return NULL;
}


/**
 * AVIO callback: read stream, skipping external headers if needed
 */
static int ffmpeg_read(void *opaque, uint8_t *buf, int buf_size)
{
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) opaque;
    uint64_t offset = data->offset;
    int max_to_copy = 0;
    int ret;

    if (data->header_insert_block) {
        if (offset < data->header_size) {
            max_to_copy = (int)(data->header_size - offset);
            if (max_to_copy > buf_size) {
                max_to_copy = buf_size;
            }
            memcpy(buf, data->header_insert_block + offset, max_to_copy);
            buf += max_to_copy;
            buf_size -= max_to_copy;
            offset += max_to_copy;
            if (!buf_size) {
                data->offset = offset;
                return max_to_copy;
            }
        }
        offset -= data->header_size;
    }

    /* when "fake" size is smaller than "real" size we need to make sure bytes_read (ret) is clamped;
     * it confuses FFmpeg in rare cases (STREAMFILE may have valid data after size) */
    if (offset + buf_size > data->size + data->header_size) {
        buf_size = data->size - offset; /* header "read" is manually inserted later */
    }

    ret = read_streamfile(buf, offset + data->start, buf_size, data->streamfile);
    if (ret > 0) {
        offset += ret;
        if (data->header_insert_block) {
            ret += max_to_copy;
        }
    }

    if (data->header_insert_block) {
        offset += data->header_size;
    }

    data->offset = offset;
    return ret;
}

/**
 * AVIO callback: write stream not needed
 */
static int ffmpeg_write(void *opaque, uint8_t *buf, int buf_size)
{
    return -1;
}

/**
 * AVIO callback: seek stream, skipping external headers if needed
 */
static int64_t ffmpeg_seek(void *opaque, int64_t offset, int whence)
{
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) opaque;
    int ret = 0;

    if (whence & AVSEEK_SIZE) {
        return data->size + data->header_size;
    }
    whence &= ~(AVSEEK_SIZE | AVSEEK_FORCE);
    /* false offsets, on reads data->start will be added */
    switch (whence) {
        case SEEK_SET:
            break;

        case SEEK_CUR:
            offset += data->offset;
            break;
            
        case SEEK_END:
            offset += data->size;
            if (data->header_insert_block)
                offset += data->header_size;
            break;
    }

    /* clamp offset; fseek returns 0 when offset > size, too */
    if (offset > data->size + data->header_size) {
        offset = data->size + data->header_size;
    }

    data->offset = offset;
    return ret;
}


/**
 * Manually init FFmpeg, from an offset.
 * Can be used if the stream has an extra header over data recognized by FFmpeg.
 */
ffmpeg_codec_data * init_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
    return init_ffmpeg_header_offset(streamFile, NULL, 0, start, size);
}


/**
 * Manually init FFmpeg, from a fake header / offset.
 *
 * Can take a fake header, to trick FFmpeg into demuxing/decoding the stream.
 * This header will be seamlessly inserted before 'start' offset, and total filesize will be 'header_size' + 'size'.
 * The header buffer will be copied and memory-managed internally.
 */
ffmpeg_codec_data * init_ffmpeg_header_offset(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size) {
	char filename[PATH_LIMIT];
    
    ffmpeg_codec_data * data;
    
    int errcode, i;
    
    int streamIndex, streamCount;
    AVStream *stream;
    AVCodecParameters *codecPar;
    
    AVRational tb;


    /* basic setup */
    g_init_ffmpeg();
    
    data = ( ffmpeg_codec_data * ) calloc(1, sizeof(ffmpeg_codec_data));
    if (!data) return NULL;
    
    streamFile->get_name( streamFile, filename, sizeof(filename) );
    
    data->streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!data->streamfile) goto fail;
    
    data->start = start;
    data->size = size;


    /* insert fake header to trick FFmpeg into demuxing/decoding the stream */
    if (header_size > 0) {
        data->header_size = header_size;
        data->header_insert_block = av_memdup(header, header_size);
        if (!data->header_insert_block) goto fail;
    }
    
    /* setup IO, attempt to autodetect format and gather some info */
    data->buffer = av_malloc(FFMPEG_DEFAULT_IO_BUFFER_SIZE);
    if (!data->buffer) goto fail;
    
    data->ioCtx = avio_alloc_context(data->buffer, FFMPEG_DEFAULT_IO_BUFFER_SIZE, 0, data, ffmpeg_read, ffmpeg_write, ffmpeg_seek);
    if (!data->ioCtx) goto fail;
    
    data->formatCtx = avformat_alloc_context();
    if (!data->formatCtx) goto fail;
    
    data->formatCtx->pb = data->ioCtx;
    
    if ((errcode = avformat_open_input(&data->formatCtx, "", NULL, NULL)) < 0) goto fail; /* autodetect */

    if ((errcode = avformat_find_stream_info(data->formatCtx, NULL)) < 0) goto fail;
    

    /* find valid audio stream inside */
    streamIndex = -1;
    streamCount = 0; /* audio streams only */
    
    for (i = 0; i < data->formatCtx->nb_streams; ++i) {
        stream = data->formatCtx->streams[i];
        codecPar = stream->codecpar;
        if (streamIndex < 0 && codecPar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streamIndex = i; /* select first audio stream found */
        } else {
            stream->discard = AVDISCARD_ALL; /* disable demuxing unneded streams */
        }
        if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO)
            streamCount++;
    }
    
    if (streamIndex < 0) goto fail;

    data->streamIndex = streamIndex;
    stream = data->formatCtx->streams[streamIndex];
    data->streamCount = streamCount;
    

    /* prepare codec and frame/packet buffers */
    data->codecCtx = avcodec_alloc_context3(NULL);
    if (!data->codecCtx) goto fail;
    
    if ((errcode = avcodec_parameters_to_context(data->codecCtx, codecPar)) < 0) goto fail;
    
    av_codec_set_pkt_timebase(data->codecCtx, stream->time_base);
    
    data->codec = avcodec_find_decoder(data->codecCtx->codec_id);
    if (!data->codec) goto fail;
    
    if ((errcode = avcodec_open2(data->codecCtx, data->codec, NULL)) < 0) goto fail;
    
    data->lastDecodedFrame = av_frame_alloc();
    if (!data->lastDecodedFrame) goto fail;
    av_frame_unref(data->lastDecodedFrame);

    data->lastReadPacket = malloc(sizeof(AVPacket));
    if (!data->lastReadPacket) goto fail;
    av_new_packet(data->lastReadPacket, 0);

    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;


    /* other setup */
    data->sampleRate = data->codecCtx->sample_rate;
    data->channels = data->codecCtx->channels;
    data->floatingPoint = 0;

    switch (data->codecCtx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            data->bitsPerSample = 8;
            break;
            
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            data->bitsPerSample = 16;
            break;
            
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            data->bitsPerSample = 32;
            break;
            
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            data->bitsPerSample = 32;
            data->floatingPoint = 1;
            break;
            
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            data->bitsPerSample = 64;
            data->floatingPoint = 1;
            break;
            
        default:
            goto fail;
    }

    data->bitrate = (int)(data->codecCtx->bit_rate);
    data->endOfStream = 0;
    data->endOfAudio = 0;

    /* try to guess frames/samples (duration isn't always set) */
    tb.num = 1; tb.den = data->codecCtx->sample_rate;
    data->totalSamples = av_rescale_q(stream->duration, stream->time_base, tb);
    if (data->totalSamples < 0)
        data->totalSamples = 0; /* caller must consider this */

    data->blockAlign = data->codecCtx->block_align;
    data->frameSize = data->codecCtx->frame_size;
    if(data->frameSize == 0) /* some formats don't set frame_size but can get on request, and vice versa */
        data->frameSize = av_get_audio_frame_duration(data->codecCtx,0);
    
    /* setup decode buffer */
    data->sampleBufferBlock = FFMPEG_DEFAULT_BUFFER_SIZE;
    data->sampleBuffer = av_malloc( data->sampleBufferBlock * (data->bitsPerSample / 8) * data->channels );
    if (!data->sampleBuffer)
        goto fail;
    

    /* setup decent seeking for faulty formats */
    errcode = init_seek(data);
    if (errcode < 0) goto fail;

    /* expose start samples to be skipped (encoder delay, usually added by MDCT-based encoders like AAC/MP3/ATRAC3/XMA/etc)
     * get after init_seek because some demuxers like AAC only fill skip_samples for the first packet */
    if (stream->start_skip_samples) /* samples to skip in the first packet */
        data->skipSamples = stream->start_skip_samples;
    else if (stream->skip_samples) /* samples to skip in any packet (first in this case), used sometimes instead (ex. AAC) */
        data->skipSamples = stream->skip_samples;

    return data;
    
fail:
    free_ffmpeg(data);
    
    return NULL;
}


/**
 * Special patching for FFmpeg's buggy seek code.
 *
 * To seek with avformat_seek_file/av_seek_frame, FFmpeg's demuxers can implement read_seek2 (newest API)
 * or read_seek (older API), with various search modes. If none are available it will use seek_frame_generic,
 * which manually reads frame by frame until the selected timestamp. However, the prev frame will be consumed
 * (so after seeking to 0 next av_read_frame will actually give the second frame and so on).
 *
 * Fortunately seek_frame_generic can use an index to find the correct position. This function reads the
 * first frame/packet and sets up index to timestamp 0. This ensures faulty demuxers will seek to 0 correctly.
 * Some formats may not seek to 0 even with this, though.
 */
static int init_seek(ffmpeg_codec_data * data) {
    int ret, ts_index, found_first = 0;
    int64_t ts = 0;
    int64_t pos = 0; /* offset */
    int size = 0; /* coded size */
    int distance = 0; /* always? */

    AVStream * stream;
    AVPacket * pkt;

    stream = data->formatCtx->streams[data->streamIndex];
    pkt = data->lastReadPacket;

    /* read_seek shouldn't need this index, but direct access to FFmpeg's internals is no good */
    /* if (data->formatCtx->iformat->read_seek || data->formatCtx->iformat->read_seek2)
        return 0; */

    /* some formats already have a proper index (e.g. M4A) */
    ts_index = av_index_search_timestamp(stream, ts, AVSEEK_FLAG_ANY);
    if (ts_index>=0)
        goto test_seek;


    /* find the first + second packets to get pos/size */
    while (1) {
        av_packet_unref(pkt);
        ret = av_read_frame(data->formatCtx, pkt);
        if (ret < 0)
            break;
        if (pkt->stream_index != data->streamIndex)
            continue; /* ignore non-selected streams */

        if (!found_first) { /* first found */
            found_first = 1;
            pos = pkt->pos;
            ts = pkt->dts;
            continue;
        } else { /* second found */
            size = pkt->pos - pos; /* coded, pkt->size is decoded size */
            break;
        }
    }
    if (!found_first)
        goto fail;

    /* in rare cases there is only one packet */
    /* if (size == 0) { size = data_end - pos; } */ /* no easy way to know, ignore (most formats don's need size) */

    /* some formats (XMA1) don't seem to have packet.dts, pretend it's 0 */
    if (ts == INT64_MIN)
        ts = 0;

    /* Some streams start with negative DTS (observed in Ogg). For Ogg seeking to negative or 0 doesn't alter the output.
     *  It does seem seeking before decoding alters a bunch of (inaudible) +-1 lower bytes though. */
    VGM_ASSERT(ts != 0, "FFMPEG: negative start_ts (%li)\n", (long)ts);
    if (ts != 0)
        ts = 0;

    /* add index 0 */
    ret = av_add_index_entry(stream, pos, ts, size, distance, AVINDEX_KEYFRAME);
    if ( ret < 0 )
        return ret;


test_seek:
    /* seek to 0 test / move back to beginning, since we just consumed packets */
    ret = avformat_seek_file(data->formatCtx, data->streamIndex, ts, ts, ts, AVSEEK_FLAG_ANY);
    if ( ret < 0 )
        return ret; /* we can't even reset_vgmstream the file */

    avcodec_flush_buffers(data->codecCtx);

    return 0;


fail:
    return -1;
}


void free_ffmpeg(ffmpeg_codec_data *data) {
    if (data == NULL)
        return;

    if (data->lastReadPacket) {
        av_packet_unref(data->lastReadPacket);
        free(data->lastReadPacket);
        data->lastReadPacket = NULL;
    }
    if (data->lastDecodedFrame) {
        av_free(data->lastDecodedFrame);
        data->lastDecodedFrame = NULL;
    }
    if (data->codecCtx) {
        avcodec_close(data->codecCtx);
        avcodec_free_context(&(data->codecCtx));
        data->codecCtx = NULL;
    }
    if (data->formatCtx) {
        avformat_close_input(&(data->formatCtx));
        data->formatCtx = NULL;
    }
    if (data->ioCtx) {
        // buffer passed in is occasionally freed and replaced.
        // the replacement must be freed as well.
        data->buffer = data->ioCtx->buffer;
        av_free(data->ioCtx);
        data->ioCtx = NULL;
    }
    if (data->buffer) {
        av_free(data->buffer);
        data->buffer = NULL;
    }
    if (data->sampleBuffer) {
        av_free(data->sampleBuffer);
        data->sampleBuffer = NULL;
    }
    if (data->header_insert_block) {
        av_free(data->header_insert_block);
        data->header_insert_block = NULL;
    }
    if (data->streamfile) {
        close_streamfile(data->streamfile);
        data->streamfile = NULL;
    }
    free(data);
}
#endif
