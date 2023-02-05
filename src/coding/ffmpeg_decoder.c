#include <math.h>
#include "coding.h"

#ifdef VGM_USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

/* opaque struct */
struct ffmpeg_codec_data {
    /*** IO internals ***/
    STREAMFILE* sf;

    uint64_t start;             // absolute start within the streamfile
    uint64_t offset;            // absolute offset within the streamfile
    uint64_t size;              // max size within the streamfile
    uint64_t logical_offset;    // computed offset FFmpeg sees (including fake header)
    uint64_t logical_size;      // computed size FFmpeg sees (including fake header)

    uint64_t header_size;       // fake header (parseable by FFmpeg) prepended on reads
    uint8_t* header_block;      // fake header data (ie. RIFF)

    /*** internal state ***/
    // config
    int stream_count;            /* FFmpeg audio streams (ignores video/etc) */
    int stream_index;
    int64_t total_samples;      /* may be 0 and innacurate */
    int64_t skip_samples;       /* number of start samples that will be skipped (encoder delay) */
    int channel_remap_set;
    int channel_remap[32];      /* map of channel > new position */
    int invert_floats_set;
    int skip_samples_set;       /* flag to know skip samples were manually added from vgmstream */
    int force_seek;             /* flags for special seeking in faulty formats */
    int bad_init;

    // FFmpeg context used for metadata
    const AVCodec* codec;

    /* FFmpeg decoder state */
    unsigned char* buffer;
    AVIOContext* ioCtx;
    AVFormatContext* formatCtx;
    AVCodecContext* codecCtx;
    AVFrame* frame;             /* last decoded frame */
    AVPacket* packet;           /* last read data packet */

    int read_packet;
    int end_of_stream;
    int end_of_audio;

    /* sample state */
    int32_t samples_discard;
    int32_t samples_consumed;
    int32_t samples_filled;
};


#define FFMPEG_DEFAULT_IO_BUFFER_SIZE  STREAMFILE_DEFAULT_BUFFER_SIZE

static volatile int g_ffmpeg_initialized = 0;

static void free_ffmpeg_config(ffmpeg_codec_data* data);
static int init_ffmpeg_config(ffmpeg_codec_data* data, int target_subsong, int reset);

/* ******************************************** */
/* INTERNAL UTILS                               */
/* ******************************************** */

/* Global FFmpeg init */
static void g_init_ffmpeg(void) {
    if (g_ffmpeg_initialized == 1) {
        while (g_ffmpeg_initialized < 2); /* active wait for lack of a better way */
    }
    else if (g_ffmpeg_initialized == 0) {
        g_ffmpeg_initialized = 1;
        av_log_set_flags(AV_LOG_SKIP_REPEATED);
        av_log_set_level(AV_LOG_ERROR);
//#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 18, 100)
//        av_register_all(); /* not needed in newer versions */
//#endif
        g_ffmpeg_initialized = 2;
    }
}

static void remap_audio(sample_t* outbuf, int sample_count, int channels, int* channel_mappings) {
    int ch_from,ch_to,s;
    sample_t temp;
    for (s = 0; s < sample_count; s++) {
        for (ch_from = 0; ch_from < channels; ch_from++) {
            if (ch_from > 32)
                continue;

            ch_to = channel_mappings[ch_from];
            if (ch_to < 1 || ch_to > 32 || ch_to > channels-1 || ch_from == ch_to)
                continue;

            temp = outbuf[s*channels + ch_from];
            outbuf[s*channels + ch_from] = outbuf[s*channels + ch_to];
            outbuf[s*channels + ch_to] = temp;
        }
    }
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
static int init_seek(ffmpeg_codec_data* data) {
    int ret, ts_index, packet_count = 0;
    int64_t ts = 0; /* seek timestamp */
    int64_t pos = 0; /* data offset */
    int size = 0; /* data size (block align) */
    int distance = 0; /* always 0 ("duration") */

    AVStream* stream = data->formatCtx->streams[data->stream_index];
    AVPacket* pkt = data->packet;


    /* read_seek shouldn't need this index, but direct access to FFmpeg's internals is no good */
    /* if (data->formatCtx->iformat->read_seek || data->formatCtx->iformat->read_seek2)
        return 0; */

    /* A few formats may have a proper index (e.g. CAF/MP4/MPC/ASF/WAV/XWMA/FLAC/MP3), but some don't
     * work with our custom index (CAF/MPC/MP4) and must skip it. Most formats need flag AVSEEK_FLAG_ANY,
     * while XWMA (with index 0 not pointing to ts 0) needs AVSEEK_FLAG_BACKWARD to seek properly, but it
     * makes OGG use the index and seek wrong instead. So for XWMA we forcefully remove the index on it's own meta. */
    ts_index = av_index_search_timestamp(stream, 0, /*AVSEEK_FLAG_BACKWARD |*/ AVSEEK_FLAG_ANY);
    if (ts_index >= 0) {
        VGM_LOG("FFMPEG: index found for init_seek\n");
        goto test_seek;
    }


    /* find the first + second packets to get pos/size */
    packet_count = 0;
    while (1) {
        av_packet_unref(pkt);
        ret = av_read_frame(data->formatCtx, pkt);
        if (ret < 0)
            break;
        if (pkt->stream_index != data->stream_index)
            continue; /* ignore non-selected streams */

        //;VGM_LOG("FFMPEG: packet %i, ret=%i, pos=%i, dts=%i\n", packet_count, ret, (int32_t)pkt->pos, (int32_t)pkt->dts);
        packet_count++;
        if (packet_count == 1) {
            pos = pkt->pos;
            ts = pkt->dts;
            continue;
        } else { /* second found */
            size = pkt->pos - pos; /* coded, pkt->size is decoded size */
            break;
        }
    }
    if (packet_count == 0)
        goto fail;

    /* happens in unseekable formats where FFmpeg doesn't even know its own position */
    if (pos < 0)
        goto fail;

    /* in rare cases there is only one packet */
    //if (size == 0) size = data_end - pos; /* no easy way to know, ignore (most formats don's need size) */

    /* some formats don't seem to have packet.dts, pretend it's 0 */
    if (ts == INT64_MIN)
        ts = 0;

    /* Some streams start with negative DTS (OGG/OPUS). For Ogg seeking to negative or 0 doesn't seem different.
     * It does seem seeking before decoding alters a bunch of (inaudible) +-1 lower bytes though.
     * Output looks correct (encoder delay, num_samples, etc) compared to libvorbis's output. */
    VGM_ASSERT(ts != 0, "FFMPEG: negative start_ts (%li)\n", (long)ts);
    if (ts != 0)
        ts = 0;


    /* add index 0 */
    ret = av_add_index_entry(stream, pos, ts, size, distance, AVINDEX_KEYFRAME);
    if ( ret < 0 )
        return ret;

test_seek:
    /* seek to 0 test + move back to beginning, since we just consumed packets */
    ret = avformat_seek_file(data->formatCtx, data->stream_index, ts, ts, ts, AVSEEK_FLAG_ANY);
    if ( ret < 0 ) {
        //char test[1000] = {0}; av_strerror(ret, test, 1000); VGM_LOG("FFMPEG: ret=%i %s\n", ret, test);
        return ret; /* we can't even reset_vgmstream the file */
    }

    avcodec_flush_buffers(data->codecCtx);

    return 0;

fail:
    return -1;
}


/* ******************************************** */
/* AVIO CALLBACKS                               */
/* ******************************************** */

/* AVIO callback: read stream, handling custom data */
static int ffmpeg_read(void* opaque, uint8_t* buf, int read_size) {
    ffmpeg_codec_data* data = opaque;
    int bytes = 0;
    int max_to_copy = 0;

    /* clamp reads */
    if (data->logical_offset + read_size > data->logical_size)
        read_size = data->logical_size - data->logical_offset;
    if (read_size == 0)
        return AVERROR_EOF;

    /* handle reads on inserted header */
    if (data->header_size && data->logical_offset < data->header_size) {
        max_to_copy = (int)(data->header_size - data->logical_offset);
        if (max_to_copy > read_size)
            max_to_copy = read_size;

        memcpy(buf, data->header_block + data->logical_offset, max_to_copy);
        buf += max_to_copy;
        read_size -= max_to_copy;
        data->logical_offset += max_to_copy;

        if (read_size == 0) {
            return max_to_copy ? max_to_copy : AVERROR_EOF; /* offset still in header */
        }
    }

    /* main read */
    bytes = read_streamfile(buf, data->offset, read_size, data->sf);
    data->logical_offset += bytes;
    data->offset += bytes;
    return bytes + max_to_copy;
}

/* AVIO callback: seek stream, handling custom data */
static int64_t ffmpeg_seek(void* opaque, int64_t offset, int whence) {
    ffmpeg_codec_data* data = opaque;
    int ret = 0;

    /* get cache'd size */
    if (whence & AVSEEK_SIZE) {
        return data->logical_size;
    }

    whence &= ~(AVSEEK_SIZE | AVSEEK_FORCE);
    /* find the final offset FFmpeg sees (within fake header + virtual size) */
    switch (whence) {
        case SEEK_SET: /* absolute */
            break;

        case SEEK_CUR: /* relative to current */
            offset += data->logical_offset;
            break;

        case SEEK_END: /* relative to file end (should be negative) */
            offset += data->logical_size;
            break;

        default:
            break;
    }

    /* clamp offset; fseek does this too */
    if (offset > data->logical_size)
        offset = data->logical_size;
    else if (offset < 0)
        offset = 0;

    /* seeks inside fake header */
    if (offset < data->header_size) {
        data->logical_offset = offset;
        data->offset = data->start;
        return ret;
    }

    /* main seek */
    data->logical_offset = offset;
    data->offset = data->start + (offset - data->header_size);
    return ret;
}

/* ******************************************** */
/* MAIN INIT/DECODER                            */
/* ******************************************** */

ffmpeg_codec_data* init_ffmpeg_offset(STREAMFILE* sf, uint64_t start, uint64_t size) {
    return init_ffmpeg_header_offset(sf, NULL,0, start,size);
}

ffmpeg_codec_data* init_ffmpeg_header_offset(STREAMFILE* sf, uint8_t* header, uint64_t header_size, uint64_t start, uint64_t size) {
    return init_ffmpeg_header_offset_subsong(sf, header, header_size, start, size, 0);
}


/**
 * Manually init FFmpeg, from a fake header / offset.
 *
 * Takes a fake header, to trick FFmpeg into demuxing/decoding the stream.
 * This header will be seamlessly inserted before 'start' offset, and total filesize will be 'header_size' + 'size'.
 * The header buffer will be copied and memory-managed internally.
 * NULL header can used given if the stream has internal data recognized by FFmpeg at offset.
 * Stream index can be passed if the file has multiple audio streams that FFmpeg can demux (1=first).
 */
ffmpeg_codec_data* init_ffmpeg_header_offset_subsong(STREAMFILE* sf, uint8_t* header, uint64_t header_size, uint64_t start, uint64_t size, int target_subsong) {
    ffmpeg_codec_data* data = NULL;
    int errcode;


    /* check values */
    if ((header && !header_size) || (!header && header_size))
        goto fail;

    if (size == 0 || start + size > get_streamfile_size(sf)) {
        vgm_asserti(size != 0, "FFMPEG: wrong start+size found: %x + %x > %x \n", (uint32_t)start, (uint32_t)size, (uint32_t)get_streamfile_size(sf));
        size = get_streamfile_size(sf) - start;
    }


    /* initial FFmpeg setup */
    g_init_ffmpeg();


    /* basic setup */
    data = calloc(1, sizeof(ffmpeg_codec_data));
    if (!data) return NULL;

    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    /* fake header to trick FFmpeg into demuxing/decoding the stream */
    if (header_size > 0) {
        data->header_size = header_size;
        data->header_block = av_memdup(header, header_size);
        if (!data->header_block) goto fail;
    }

    data->start = start;
    data->offset = data->start;
    data->size = size;
    data->logical_offset = 0;
    data->logical_size = data->header_size + data->size;


    /* setup FFmpeg's internals, attempt to autodetect format and gather some info */
    errcode = init_ffmpeg_config(data, target_subsong, 0);
    if (errcode < 0) goto fail;

    /* reset non-zero values */
    data->read_packet = 1;

    /* setup other values */
    {
        AVStream* stream = data->formatCtx->streams[data->stream_index];
        AVRational tb = {0};

        tb.num = 1; tb.den = data->codecCtx->sample_rate;

#if 0
        /* derive info */
        data->sampleRate = data->codecCtx->sample_rate;
        data->channels = data->codecCtx->ch_layout.nb_channels; //data->codecCtx->channels;
        data->bitrate = (int)(data->codecCtx->bit_rate);
        data->blockAlign = data->codecCtx->block_align;
        data->frameSize = data->codecCtx->frame_size;
        if(data->frameSize == 0) /* some formats don't set frame_size but can get on request, and vice versa */
            data->frameSize = av_get_audio_frame_duration(data->codecCtx,0);
#endif

        /* try to guess frames/samples (duration isn't always set) */
        data->total_samples = av_rescale_q(stream->duration, stream->time_base, tb);
        if (data->total_samples < 0)
            data->total_samples = 0;

        /* read start samples to be skipped (encoder delay), info only.
         * Not too reliable though, see ffmpeg_set_skip_samples */
        if (stream->start_time && stream->start_time != AV_NOPTS_VALUE)
            data->skip_samples = av_rescale_q(stream->start_time, stream->time_base, tb);
        if (data->skip_samples < 0)
            data->skip_samples = 0;

#if 0
        //LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 64, 100)
        /* exposed before but not too reliable either */
        else if (stream->start_skip_samples) /* samples to skip in the first packet */
            data->skip_samples = stream->start_skip_samples;
        else if (stream->skip_samples) /* samples to skip in any packet (first in this case), used sometimes instead (ex. AAC) */
            data->skip_samples = stream->skip_samples;
#endif

        /* check ways to skip encoder delay/padding, for debugging purposes (some may be old/unused/encoder only/etc) */
        //VGM_ASSERT(data->codecCtx->internal->skip_samples > 0, ...); /* for codec use, not accessible */
        VGM_ASSERT(data->codecCtx->delay > 0, "FFMPEG: delay %i\n", (int)data->codecCtx->delay);//delay: OPUS
        VGM_ASSERT(stream->codecpar->initial_padding > 0, "FFMPEG: initial_padding %i\n", (int)stream->codecpar->initial_padding);//delay: OPUS
        VGM_ASSERT(stream->codecpar->trailing_padding > 0, "FFMPEG: trailing_padding %i\n", (int)stream->codecpar->trailing_padding);
        VGM_ASSERT(stream->codecpar->seek_preroll > 0, "FFMPEG: seek_preroll %i\n", (int)stream->codecpar->seek_preroll);//seek delay: OPUS
        VGM_ASSERT(stream->start_time > 0, "FFMPEG: start_time %i\n", (int)stream->start_time); //delay
#if 0
        //LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 64, 100)
        VGM_ASSERT(stream->first_discard_sample > 0, "FFMPEG: first_discard_sample %i\n", (int)stream->first_discard_sample); //padding: MP3
        VGM_ASSERT(stream->last_discard_sample > 0, "FFMPEG: last_discard_sample %i\n", (int)stream->last_discard_sample); //padding: MP3
        VGM_ASSERT(stream->skip_samples > 0, "FFMPEG: skip_samples %i\n", (int)stream->skip_samples); //delay: MP4
        VGM_ASSERT(stream->start_skip_samples > 0, "FFMPEG: start_skip_samples %i\n", (int)stream->start_skip_samples); //delay: MP3
#endif
        /* also negative timestamp for formats like OGG/OPUS */
        /* not using it: BINK, FLAC, ATRAC3, XMA, MPC, WMA (may use internal skip samples) */
    }


    /* setup decent seeking for faulty formats */
    errcode = init_seek(data);
    if (errcode < 0) {
        VGM_LOG("FFMPEG: can't init_seek, error=%i (using force_seek)\n", errcode);
        ffmpeg_set_force_seek(data);
    }

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}

static int init_ffmpeg_config(ffmpeg_codec_data* data, int target_subsong, int reset) {
    int errcode = 0;

    /* basic IO/format setup */
    data->buffer = av_malloc(FFMPEG_DEFAULT_IO_BUFFER_SIZE);
    if (!data->buffer) goto fail;

    data->ioCtx = avio_alloc_context(data->buffer, FFMPEG_DEFAULT_IO_BUFFER_SIZE, 0, data, ffmpeg_read, 0, ffmpeg_seek);
    if (!data->ioCtx) goto fail;

    data->formatCtx = avformat_alloc_context();
    if (!data->formatCtx) goto fail;

    data->formatCtx->pb = data->ioCtx;

    //data->inputFormatCtx = av_find_input_format("h264"); /* set directly? */
    /* on reset could use AVFormatContext.iformat to reload old format too */

    errcode = avformat_open_input(&data->formatCtx, NULL /*""*/, NULL, NULL);
    if (errcode < 0) goto fail;

    errcode = avformat_find_stream_info(data->formatCtx, NULL);
    if (errcode < 0) goto fail;

    /* find valid audio stream and set other streams to discard */
    {
        int i, stream_index, stream_count;

        stream_index = -1;
        stream_count = 0;
        if (reset)
            stream_index = data->stream_index;

        for (i = 0; i < data->formatCtx->nb_streams; ++i) {
            AVStream* stream = data->formatCtx->streams[i];

            if (stream->codecpar && stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                stream_count++;

                /* select Nth audio stream if specified, or first one */
                if (stream_index < 0 || (target_subsong > 0 && stream_count == target_subsong)) {
                    stream_index = i;
                }
            }

            if (i != stream_index)
                stream->discard = AVDISCARD_ALL; /* disable demuxing for other streams */
        }
        if (stream_count < target_subsong) goto fail;
        if (stream_index < 0) goto fail;

        data->stream_index = stream_index;
        data->stream_count = stream_count;
    }

    /* setup codec with stream info */
    data->codecCtx = avcodec_alloc_context3(NULL);
    if (!data->codecCtx) goto fail;

    errcode = avcodec_parameters_to_context(data->codecCtx, data->formatCtx->streams[data->stream_index]->codecpar);
    if (errcode < 0) goto fail;

    /* deprecated and seemingly not needed */
    //av_codec_set_pkt_timebase(data->codecCtx, stream->time_base);

    /* not useddeprecated and seemingly not needed */
    data->codec = avcodec_find_decoder(data->codecCtx->codec_id);
    if (!data->codec) goto fail;

    errcode = avcodec_open2(data->codecCtx, data->codec, NULL);
    if (errcode < 0) goto fail;

    /* prepare codec and frame/packet buffers */
    data->packet = av_malloc(sizeof(AVPacket)); /* av_packet_alloc? */
    if (!data->packet) goto fail;
    av_new_packet(data->packet, 0);
    //av_packet_unref?

    data->frame = av_frame_alloc();
    if (!data->frame) goto fail;
    av_frame_unref(data->frame);


    return 0;
fail:
    if (errcode < 0)
        return errcode;
    return -1;
}

/* decodes a new frame to internal data */
static int decode_ffmpeg_frame(ffmpeg_codec_data* data) {
    int errcode;
    int frame_error = 0;


    if (data->bad_init) {
        goto fail;
    }

    /* ignore once file is done (but not on EOF as FFmpeg can output samples until end_of_audio) */
    if (/*data->end_of_stream ||*/ data->end_of_audio) {
        VGM_LOG("FFMPEG: decode after end of audio\n");
        goto fail;
    }


    /* read data packets until valid is found */
    while (data->read_packet && !data->end_of_audio) {
        if (!data->end_of_stream) {
            /* reset old packet */
            av_packet_unref(data->packet);

            /* read encoded data from demuxer into packet */
            errcode = av_read_frame(data->formatCtx, data->packet);
            if (errcode < 0) {
                if (errcode == AVERROR_EOF) {
                    data->end_of_stream = 1; /* no more data to read (but may "drain" samples) */
                }
                else {
                    VGM_LOG("FFMPEG: av_read_frame errcode=%i\n", errcode);
                    frame_error = 1; //goto fail;
                }

                if (data->formatCtx->pb && data->formatCtx->pb->error) {
                    VGM_LOG("FFMPEG: pb error=%i\n", data->formatCtx->pb->error);
                    frame_error = 1; //goto fail;
                }
            }

            /* ignore non-selected streams */
            if (data->packet->stream_index != data->stream_index)
                continue;
        }

        /* send encoded data to frame decoder (NULL at EOF to "drain" samples below) */
        errcode = avcodec_send_packet(data->codecCtx, data->end_of_stream ? NULL : data->packet);
        if (errcode < 0) {
            if (errcode != AVERROR(EAGAIN)) {
                VGM_LOG("FFMPEG: avcodec_send_packet errcode=%i\n", errcode);
                frame_error = 1; //goto fail;
            }
        }

        data->read_packet = 0; /* got data */
    }

    /* decode frame samples from sent packet or "drain" samples*/
    if (!frame_error) {
        /* receive uncompressed sample data from decoded frame */
        errcode = avcodec_receive_frame(data->codecCtx, data->frame);
        if (errcode < 0) {
            if (errcode == AVERROR_EOF) {
                data->end_of_audio = 1; /* no more audio, file is fully decoded */
            }
            else if (errcode == AVERROR(EAGAIN)) {
                data->read_packet = 1; /* 0 samples, request more encoded data */
            }
            else {
                VGM_LOG("FFMPEG: avcodec_receive_frame errcode=%i\n", errcode);
                frame_error = 1;//goto fail;
            }
        }
    }

    /* on frame_error simply uses current frame (possibly with nb_samples=0), which mirrors ffmpeg's output
     * (ex. BlazBlue X360 022_btl_az.xwb) */


    data->samples_consumed = 0;
    data->samples_filled = data->frame->nb_samples;
    return 1;
fail:
    return 0;
}


/* When casting float to int value is simply truncated:
 * - 0.0000518798828125 * 32768.0f = 1.7f, (int)1.7 = 1, (int)-1.7 = -1
 *   (instead of 1.7 = 2, -1.7 = -2)
 *
 * Alts for more accurate rounding could be:
 * - (int)floor(f32 * 32768.0) //not quite ok negatives
 * - (int)floor(f32 * 32768.0f + 0.5f) //Xiph Vorbis style
 * - (int)(f32 < 0 ? f32 - 0.5f : f + 0.5f)
 * - (((int) (f1 + 32768.5)) - 32768)
 * - etc
 * but since +-1 isn't really audible we'll just cast as it's the fastest.
 *
 * Regular C float-to-int casting ("int i = (int)f") is somewhat slow due to IEEE
 * float requirements, but C99 adds some faster-but-less-precise casting functions
 * we try to use (returning "long", though). They work ok without "fast float math" compiler
 * flags, but probably should be enabled anyway to ensure no extra IEEE checks are needed.
 * MSVC added this in VS2015 (_MSC_VER 1900) but don't seem correctly optimized and is very slow.
 */
static inline int float_to_int(float val) {
#if defined(_MSC_VER)
    return (int)val;
#else
    return lrintf(val);
#endif
}
static inline int double_to_int(double val) {
#if defined(_MSC_VER)
    return (int)val;
#else
    return lrint(val); /* returns long tho */
#endif
}

/* sample copy helpers, using different functions to minimize branches.
 *
 * in theory, small optimizations like *outbuf++ vs outbuf[i] or alt clamping
 * would matter for performance, but in practice aren't very noticeable;
 * keep it simple for now until more tests are done.
 *
 * in normal (interleaved) formats samples are laid out straight
 *  (ibuf[s*chs+ch], ex. 4ch with 4s: 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3)
 * in "p" (planar) formats samples are in planes per channel
 *  (ibuf[ch][s], ex. 4ch with 4s: 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3)
 *
 * alt float clamping:
 *  clamp_float(f32)
 *     int s16 = (int)(f32 * 32768.0f);
 *     if ((unsigned)(s16 + 0x8000) & 0xFFFF0000)
 *         s16 = (s16 >> 31) ^ 0x7FFF;
 */

static void samples_silence_s16(sample_t* obuf, int ochs, int samples) {
    int s, total_samples = samples * ochs;
    for (s = 0; s < total_samples; s++) {
        obuf[s] = 0; /* memset'd */
    }
}

static void samples_u8_to_s16(sample_t* obuf, uint8_t* ibuf, int ichs, int samples, int skip) {
    int s, total_samples = samples * ichs;
    for (s = 0; s < total_samples; s++) {
        obuf[s] = ((int)ibuf[skip*ichs + s] - 0x80) << 8;
    }
}
static void samples_u8p_to_s16(sample_t* obuf, uint8_t** ibuf, int ichs, int samples, int skip) {
    int s, ch;
    for (ch = 0; ch < ichs; ch++) {
        for (s = 0; s < samples; s++) {
            obuf[s*ichs + ch] = ((int)ibuf[ch][skip + s] - 0x80) << 8;
        }
    }
}
static void samples_s16_to_s16(sample_t* obuf, int16_t* ibuf, int ichs, int samples, int skip) {
    int s, total_samples = samples * ichs;
    for (s = 0; s < total_samples; s++) {
        obuf[s] = ibuf[skip*ichs + s]; /* maybe should mempcy */
    }
}
static void samples_s16p_to_s16(sample_t* obuf, int16_t** ibuf, int ichs, int samples, int skip) {
    int s, ch;
    for (ch = 0; ch < ichs; ch++) {
        for (s = 0; s < samples; s++) {
            obuf[s*ichs + ch] = ibuf[ch][skip + s];
        }
    }
}
static void samples_s32_to_s16(sample_t* obuf, int32_t* ibuf, int ichs, int samples, int skip) {
    int s, total_samples = samples * ichs;
    for (s = 0; s < total_samples; s++) {
        obuf[s] = ibuf[skip*ichs + s] >> 16;
    }
}
static void samples_s32p_to_s16(sample_t* obuf, int32_t** ibuf, int ichs, int samples, int skip) {
    int s, ch;
    for (ch = 0; ch < ichs; ch++) {
        for (s = 0; s < samples; s++) {
            obuf[s*ichs + ch] = ibuf[ch][skip + s] >> 16;
        }
    }
}
static void samples_flt_to_s16(sample_t* obuf, float* ibuf, int ichs, int samples, int skip, int invert) {
    int s, total_samples = samples * ichs;
    float scale = invert ? -32768.0f : 32768.0f;
    for (s = 0; s < total_samples; s++) {
        obuf[s] = clamp16(float_to_int(ibuf[skip*ichs + s] * scale));
    }
}
static void samples_fltp_to_s16(sample_t* obuf, float** ibuf, int ichs, int samples, int skip, int invert) {
    int s, ch;
    float scale = invert ? -32768.0f : 32768.0f;
    for (ch = 0; ch < ichs; ch++) {
        for (s = 0; s < samples; s++) {
            obuf[s*ichs + ch] = clamp16(float_to_int(ibuf[ch][skip + s] * scale));
        }
    }
}
static void samples_dbl_to_s16(sample_t* obuf, double* ibuf, int ichs, int samples, int skip) {
    int s, total_samples = samples * ichs;
    for (s = 0; s < total_samples; s++) {
        obuf[s] = clamp16(double_to_int(ibuf[skip*ichs + s] * 32768.0));
    }
}
static void samples_dblp_to_s16(sample_t* obuf, double** inbuf, int ichs, int samples, int skip) {
    int s, ch;
    for (ch = 0; ch < ichs; ch++) {
        for (s = 0; s < samples; s++) {
            obuf[s*ichs + ch] = clamp16(double_to_int(inbuf[ch][skip + s] * 32768.0));
        }
    }
}

static void copy_samples(ffmpeg_codec_data* data, sample_t* outbuf, int samples_to_do) {
    int channels = data->codecCtx->ch_layout.nb_channels; //data->codecCtx->channels;
    int is_planar = av_sample_fmt_is_planar(data->codecCtx->sample_fmt) && (channels > 1);
    void* ibuf;

    if (is_planar) {
        ibuf = data->frame->extended_data;
    }
    else {
        ibuf = data->frame->data[0];
    }

    switch (data->codecCtx->sample_fmt) {
        /* unused? */
        case AV_SAMPLE_FMT_U8P:  if (is_planar) { samples_u8p_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break; }
        case AV_SAMPLE_FMT_U8:   samples_u8_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break;
        /* common */
        case AV_SAMPLE_FMT_S16P: if (is_planar) { samples_s16p_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break; }
        case AV_SAMPLE_FMT_S16:  samples_s16_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break;
        /* possibly FLAC and other lossless codecs */
        case AV_SAMPLE_FMT_S32P: if (is_planar) { samples_s32p_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break; }
        case AV_SAMPLE_FMT_S32:  samples_s32_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break;
        /* mainly MDCT-like codecs (Ogg, AAC, etc) */
        case AV_SAMPLE_FMT_FLTP: if (is_planar) { samples_fltp_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed, data->invert_floats_set); break; }
        case AV_SAMPLE_FMT_FLT:  samples_flt_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed, data->invert_floats_set); break;
        /* possibly PCM64 only (not enabled) */
        case AV_SAMPLE_FMT_DBLP: if (is_planar) { samples_dblp_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break; }
        case AV_SAMPLE_FMT_DBL:  samples_dbl_to_s16(outbuf, ibuf, channels, samples_to_do, data->samples_consumed); break;
        default:
            break;
    }

    if (data->channel_remap_set)
        remap_audio(outbuf, samples_to_do, channels, data->channel_remap);
}

/* decode samples of any kind of FFmpeg format */
void decode_ffmpeg(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels) {
    ffmpeg_codec_data* data = vgmstream->codec_data;


    while (samples_to_do > 0) {

        if (data->samples_consumed < data->samples_filled) {
            /* consume samples */
            int samples_to_get = (data->samples_filled - data->samples_consumed);

            if (data->samples_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_discard)
                    samples_to_get = data->samples_discard;
                data->samples_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do)
                    samples_to_get = samples_to_do;

                copy_samples(data, outbuf, samples_to_get);

                samples_to_do -= samples_to_get;
                outbuf += samples_to_get * channels;
            }

            /* mark consumed samples */
            data->samples_consumed += samples_to_get;
        }
        else {
            int ok = decode_ffmpeg_frame(data);
            if (!ok) goto decode_fail;
        }
    }

    return;

decode_fail:
    VGM_LOG("FFMPEG: decode fail, missing %i samples\n", samples_to_do);
    samples_silence_s16(outbuf, channels, samples_to_do);
}


/* ******************************************** */
/* UTILS                                        */
/* ******************************************** */

void reset_ffmpeg(ffmpeg_codec_data* data) {
    seek_ffmpeg(data, 0);
}

void seek_ffmpeg(ffmpeg_codec_data* data, int32_t num_sample) {
    if (!data) return;

    /* Start from 0 and discard samples until sample (slower but not too noticeable).
     * Due to many FFmpeg quirks seeking to a sample is erratic at best in most formats. */

    if (data->force_seek) {
        int errcode;

        /* kill+redo everything to allow seeking for extra-buggy formats,
         * kinda horrid but seems fast enough and very few formats need this */

        free_ffmpeg_config(data);

        data->offset = data->start;
        data->logical_offset = 0;

        errcode = init_ffmpeg_config(data, 0, 1);
        if (errcode < 0) goto fail;
    }
    else {
        avformat_seek_file(data->formatCtx, data->stream_index, 0, 0, 0, AVSEEK_FLAG_ANY);
        avcodec_flush_buffers(data->codecCtx);
    }

    data->samples_consumed = 0;
    data->samples_filled = 0;
    data->samples_discard = num_sample;

    data->read_packet = 1;
    data->end_of_stream = 0;
    data->end_of_audio = 0;

    /* consider skip samples (encoder delay), if manually set */
    if (data->skip_samples_set) {
        data->samples_discard += data->skip_samples;
        /* internally FFmpeg may skip (skip_samples/start_skip_samples) too */
    }

    return;
fail:
    VGM_LOG("FFMPEG: error during force_seek\n");
    data->bad_init = 1; /* internals were probably free'd */
}


static void free_ffmpeg_config(ffmpeg_codec_data* data) {
    if (data == NULL)
        return;

    if (data->packet) {
        av_packet_unref(data->packet);
        av_free(data->packet);
        data->packet = NULL;
    }
    if (data->frame) {
        av_frame_unref(data->frame);
        av_free(data->frame);
        data->frame = NULL;
    }
    if (data->codecCtx) {
        avcodec_close(data->codecCtx);
        avcodec_free_context(&data->codecCtx);
        data->codecCtx = NULL;
    }
    if (data->formatCtx) {
        avformat_close_input(&data->formatCtx);
        //avformat_free_context(data->formatCtx); /* done in close_input */
        data->formatCtx = NULL;
    }
    if (data->ioCtx) {
        /* buffer passed in is occasionally freed and replaced.
         * the replacement must be free'd as well (below) */
        data->buffer = data->ioCtx->buffer;
        avio_context_free(&data->ioCtx);
        //av_free(data->ioCtx); /* done in context_free (same thing) */
        data->ioCtx = NULL;
    }
    if (data->buffer) {
        av_free(data->buffer);
        data->buffer = NULL;
    }

    //todo avformat_find_stream_info may cause some Win Handle leaks? related to certain option
}

void free_ffmpeg(ffmpeg_codec_data* data) {
    if (data == NULL)
        return;

    free_ffmpeg_config(data);

    if (data->header_block) {
        av_free(data->header_block);
        data->header_block = NULL;
    }

    close_streamfile(data->sf);
    free(data);
}


/**
 * Sets the number of samples to skip at the beginning of the stream (encoder delay), needed by some "gapless" formats.
 * - should be used at the beginning of the stream
 * - should use only if/when FFmpeg's format is known to botch encoder delay.
 *
 * encoder delay in FFmpeg is handled in multiple ways:
 * - avstream/internal->start_skip_samples: skip in the first packet *if* pts=0 (set in MP3 only?)
 * - avstream/internal->skip_samples: skip in any packet (set in AAC encoded by libfaac, OPUS, MP3 in SWF, MOV/MP4)
 * - avstream->start_time: usually set same as skip_samples but in pts, info only (most of the above but OPUS)
 * - codecCtx->delay: seems equivalent to skip_samples, info only (OPUS)
 * - negative timestamp: Xiph style (Ogg Vorbis/Opus only?).
 * First two are only exposed in FFmpeg v4.4<, meaning you can't override buggy values after that.
 * But since FFmpeg only does encoder delay for a handful of formats, shouldn't matter much.
 * May need to detect exact versions if they start fixing formats.
 */
void ffmpeg_set_skip_samples(ffmpeg_codec_data* data, int skip_samples) {
    if (!data || !data->formatCtx || !skip_samples)
        return;

    /* let FFmpeg handle (may need an option to force override?) */
    if (data->skip_samples) {
        VGM_ASSERT(data->skip_samples != skip_samples,
                "FMPEG: ignored skip_samples %i, already set %i\n", skip_samples, (int)data->skip_samples);
        return;
    }

#if 0
    {
        AVStream* stream = data->formatCtx->streams[data->stream_index];
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 64, 100)
        stream->start_skip_samples = 0;
        stream->skip_samples = 0;
#else
        //stream->start_time = 0; /* info only = useless */
#endif
    }
#endif

    /* set skip samples with our internal discard */
    data->skip_samples_set = 1;
    data->samples_discard = skip_samples;
    data->skip_samples = skip_samples;
}

/* returns channel layout if set */
uint32_t ffmpeg_get_channel_layout(ffmpeg_codec_data* data) {
    if (!data || !data->codecCtx) return 0;

    /* old */
    //return (uint32_t)data->codecCtx->channel_layout; /* uint64 but there ain't so many speaker mappings */

    /* new API is not very clear so maybe there is a better way */
    if (data->codecCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        return 0;
    if (data->codecCtx->ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
        return (uint32_t)data->codecCtx->ch_layout.u.mask;
    }

    /* other options: not handled for now */
    return 0;
}


/* yet another hack to fix codecs that encode channels in different order and reorder on decoder
 * but FFmpeg doesn't do it automatically
 * (maybe should be done via mixing, but could clash with other stuff?) */
void ffmpeg_set_channel_remapping(ffmpeg_codec_data* data, int *channel_remap) {
    int i;

    if (data->codecCtx->ch_layout.nb_channels > 32)
        return;

    for (i = 0; i < data->codecCtx->ch_layout.nb_channels; i++) {
        data->channel_remap[i] = channel_remap[i];
    }
    data->channel_remap_set = 1;
}

const char* ffmpeg_get_codec_name(ffmpeg_codec_data* data) {
    if (!data || !data->codec)
        return NULL;
    if (data->codec->long_name)
        return data->codec->long_name;
    if (data->codec->name)
        return data->codec->name;
    return NULL;
}

void ffmpeg_set_force_seek(ffmpeg_codec_data* data) {
    if (!data)
        return;
    /* some formats like Smacker are so buggy that any seeking is impossible (even on video players),
     * or MPC with an incorrectly parsed seek table (using as 0 some non-0 seek offset).
     * whatever, we'll just kill and reconstruct FFmpeg's config every time */
    data->force_seek = 1;
    reset_ffmpeg(data); /* reset state from trying to seek */
    //stream = data->formatCtx->streams[data->stream_index];
}

void ffmpeg_set_invert_floats(ffmpeg_codec_data* data) {
    if (!data)
        return;
    data->invert_floats_set = 1;
}

const char* ffmpeg_get_metadata_value(ffmpeg_codec_data* data, const char* key) {
    AVDictionary* avd;
    AVDictionaryEntry* avde = NULL;

    if (!data || !data->codec)
        return NULL;

    avd = data->formatCtx->streams[data->stream_index]->metadata; /* per stream (like Ogg) */
    if (!avd)
        avd = data->formatCtx->metadata; /* per format (like Flac) */
    if (!avd)
        return NULL;

    avde = av_dict_get(avd, key, NULL, AV_DICT_IGNORE_SUFFIX);
    if (!avde)
        return NULL;

    return avde->value;
}

int32_t ffmpeg_get_samples(ffmpeg_codec_data* data) {
    if (!data)
        return 0;
    return (int32_t)data->total_samples;
}

int ffmpeg_get_sample_rate(ffmpeg_codec_data* data) {
    if (!data || !data->codecCtx)
        return 0;
    return data->codecCtx->sample_rate;
}

int ffmpeg_get_channels(ffmpeg_codec_data* data) {
    if (!data || !data->codecCtx)
        return 0;
    return data->codecCtx->ch_layout.nb_channels; //data->codecCtx->channels;
}

int ffmpeg_get_subsong_count(ffmpeg_codec_data* data) {
    if (!data)
        return 0;
    return data->stream_count;
}


STREAMFILE* ffmpeg_get_streamfile(ffmpeg_codec_data* data) {
    if (!data) return NULL;
    return data->sf;
}
#endif
