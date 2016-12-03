#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"

#ifdef VGM_USE_FFMPEG

/* internal sizes, can be any value */
#define FFMPEG_DEFAULT_BLOCK_SIZE 2048
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

    ffmpeg_codec_data *data = init_ffmpeg_offset(streamFile, start, size);
    if (!data) return NULL;
    
    vgmstream = allocate_vgmstream(data->channels, 0);
    if (!vgmstream) goto fail;
    
    vgmstream->loop_flag = 0;
    vgmstream->codec_data = data;
    vgmstream->channels = data->channels;
    vgmstream->sample_rate = data->sampleRate;
    vgmstream->num_samples = data->totalFrames;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_FFmpeg;

    /* this may happen for some streams */
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
    if (offset > data->size + data->header_size)
        offset = data->size + data->header_size;
    return data->offset = offset;
}


/**
 * Manually init FFmpeg only, from an offset.
 * Can be used if the stream has an extra header over data recognized by FFmpeg.
 */
ffmpeg_codec_data * init_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
    return init_ffmpeg_faux_riff(streamFile, -1, start, size, 0);
}

/**
 * Manually init FFmpeg only, from an offset / fake RIFF.
 * Can insert a fake RIFF header, to trick FFmpeg into demuxing/decoding the stream.
 */
ffmpeg_codec_data * init_ffmpeg_faux_riff(STREAMFILE *streamFile, int64_t fmt_offset, uint64_t start, uint64_t size, int big_endian) {
	char filename[PATH_LIMIT];
    
    ffmpeg_codec_data * data;
    
    int errcode, i;
    
    int streamIndex;
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


    /* insert fake RIFF header to trick FFmpeg into demuxing/decoding the stream */
    if (fmt_offset > 0) {
        int max_header_size = (int)(start - fmt_offset);
        uint8_t *p;
        if (max_header_size < 18) goto fail;
        data->header_insert_block = p = av_malloc(max_header_size + 8 + 4 + 8 + 8);
        if (!data->header_insert_block) goto fail;
        if (read_streamfile(p + 8 + 4 + 8, fmt_offset, max_header_size, streamFile) != max_header_size) goto fail;
        if (big_endian) {
            p += 8 + 4 + 8;
            put_16bitLE(p, get_16bitBE(p));
            put_16bitLE(p + 2, get_16bitBE(p + 2));
            put_32bitLE(p + 4, get_32bitBE(p + 4));
            put_32bitLE(p + 8, get_32bitBE(p + 8));
            put_16bitLE(p + 12, get_16bitBE(p + 12));
            put_16bitLE(p + 14, get_16bitBE(p + 14));
            put_16bitLE(p + 16, get_16bitBE(p + 16));
            p -= 8 + 4 + 8;
        }
        data->header_size = 8 + 4 + 8 + 8 + 18 + get_16bitLE(p + 8 + 4 + 8 + 16);
        // Meh, dunno how to handle swapping the extra data
        // FFmpeg doesn't need most of this data anyway
        if ((unsigned)(get_16bitLE(p + 8 + 4 + 8) - 0x165) < 2)
            memset(p + 8 + 4 + 8 + 18, 0, 34);
        
        // Fill out the RIFF structure
        memcpy(p, "RIFF", 4);
        put_32bitLE(p + 4, data->header_size + size - 8);
        memcpy(p + 8, "WAVE", 4);
        memcpy(p + 12, "fmt ", 4);
        put_32bitLE(p + 16, 18 + get_16bitLE(p + 8 + 4 + 8 + 16));
        memcpy(p + data->header_size - 8, "data", 4);
        put_32bitLE(p + data->header_size - 4, size);
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
    
    for (i = 0; i < data->formatCtx->nb_streams; ++i) {
        stream = data->formatCtx->streams[i];
        codecPar = stream->codecpar;
        if (streamIndex < 0 && codecPar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streamIndex = i; /* select first audio stream found */
        } else {
            stream->discard = AVDISCARD_ALL; /* disable demuxing unneded streams */
        }
    }
    
    if (streamIndex < 0) goto fail;
    
    data->streamIndex = streamIndex;
    stream = data->formatCtx->streams[streamIndex];
    

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
    data->totalFrames = av_rescale_q(stream->duration, stream->time_base, tb);
    if (data->totalFrames < 0)
        data->totalFrames = 0; /* caller must consider this */

    /* setup decode buffer */
    data->samplesPerBlock = FFMPEG_DEFAULT_BLOCK_SIZE;
    data->sampleBuffer = av_malloc( data->samplesPerBlock * (data->bitsPerSample / 8) * data->channels );
    if (!data->sampleBuffer)
        goto fail;
    

    /* setup decent seeking for faulty formats */
    errcode = init_seek(data);
    if (errcode < 0) goto fail;


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
    int64_t pos; /* offset */
    int size; /* coded size */
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
            goto fail;
        if (pkt->stream_index != data->streamIndex)
            continue; /* ignore non-selected streams */

        if (!found_first) { /* first found */
            found_first = 1;
            pos = pkt->pos;
            continue;
        } else { /* second found */
            size = pkt->pos - pos; /* coded, pkt->size is decoded size */
            break;
        }
    }

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
