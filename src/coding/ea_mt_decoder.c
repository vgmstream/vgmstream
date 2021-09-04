#include "coding.h"

#include "ea_mt_decoder_utk.h"

/* Decodes EA MicroTalk (speech codec) using utkencode lib (slightly modified for vgmstream).
 * EA separates MT10:1 and MT5:1 (bigger frames), but apparently are the same
 * with different encoding parameters. Later revisions may have PCM blocks (rare).
 *
 * Decoder by Andrew D'Addesio: https://github.com/daddesio/utkencode
 * Info: http://wiki.niotso.org/UTK
 */


//#define UTK_MAKE_U32(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define UTK_ROUND(x) ((x) >= 0.0f ? ((x)+0.5f) : ((x)-0.5f))
#define UTK_MIN(x,y) ((x)<(y)?(x):(y))
#define UTK_MAX(x,y) ((x)>(y)?(x):(y))
#define UTK_CLAMP(x,min,max) UTK_MIN(UTK_MAX(x,min),max)

#define UTK_BUFFER_SIZE 0x1000

struct ea_mt_codec_data {
    STREAMFILE *streamfile;
    uint8_t buffer[UTK_BUFFER_SIZE];
    off_t offset;
    off_t loop_offset;
    int loop_sample;

    int pcm_blocks;
    int samples_filled;
    int samples_used;
    int samples_done;
    int samples_discard;
    void* utk_context;
};

static size_t ea_mt_read_callback(void *dest, int size, void *arg);

ea_mt_codec_data* init_ea_mt(int channels, int pcm_blocks) {
    return init_ea_mt_loops(channels, pcm_blocks, 0, NULL);
}

ea_mt_codec_data* init_ea_mt_loops(int channels, int pcm_blocks, int loop_sample, off_t *loop_offsets) {
    ea_mt_codec_data* data = NULL;
    int i;

    data = calloc(channels, sizeof(ea_mt_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (i = 0; i < channels; i++) {
        data[i].utk_context = calloc(1, sizeof(UTKContext));
        if (!data[i].utk_context) goto fail;
        utk_init(data[i].utk_context);

        data[i].pcm_blocks = pcm_blocks;
        data[i].loop_sample = loop_sample;
        if (loop_offsets)
            data[i].loop_offset = loop_offsets[i];

        utk_set_callback(data[i].utk_context, data[i].buffer, UTK_BUFFER_SIZE, &data[i], &ea_mt_read_callback);
    }

    return data;

fail:
    free_ea_mt(data, channels);
    return NULL;
}

void decode_ea_mt(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    int i;
    ea_mt_codec_data* data = vgmstream->codec_data;
    ea_mt_codec_data* ch_data = &data[channel];
    UTKContext* ctx = ch_data->utk_context;
    int samples_done = 0;


    while (samples_done < samples_to_do) {

        if (ch_data->samples_filled) {
            /* consume current frame */
            int samples_to_get = ch_data->samples_filled;

            /* don't go past loop, to reset decoder */
            if (ch_data->loop_sample > 0 && ch_data->samples_done < ch_data->loop_sample &&
                    ch_data->samples_done + samples_to_get > ch_data->loop_sample)
                samples_to_get = ch_data->loop_sample - ch_data->samples_done;

            if (ch_data->samples_discard) {
                /* discard samples for looping */
                if (samples_to_get > ch_data->samples_discard)
                    samples_to_get = ch_data->samples_discard;
                ch_data->samples_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do - samples_done)
                    samples_to_get = samples_to_do - samples_done;

                for (i = ch_data->samples_used; i < ch_data->samples_used + samples_to_get; i++) {
                    int pcm = UTK_ROUND(ctx->decompressed_frame[i]);
                    outbuf[0] = (int16_t)UTK_CLAMP(pcm, -32768, 32767);
                    outbuf += channelspacing;
                }

                samples_done += samples_to_get;
            }

            /* mark consumed samples */
            ch_data->samples_used += samples_to_get;
            ch_data->samples_filled -= samples_to_get;
            ch_data->samples_done += samples_to_get;

            /* Loops in EA-MT are done with fully separate intro/loop substreams. We must
             * notify the decoder when a new substream begins (even with looping disabled). */
            if (ch_data->loop_sample > 0 && ch_data->samples_done == ch_data->loop_sample) {
                ch_data->samples_filled = 0;
                ch_data->samples_discard = 0;

                /* offset is usually at loop_offset here, but not always (ex. loop_sample < 432) */
                ch_data->offset = ch_data->loop_offset;
                utk_set_ptr(ctx, 0, 0); /* reset the buffer reader */
                utk_reset(ctx); /* decoder init (all fields must be reset, for some edge cases) */
            }
        }
        else {
            /* new frame */
            if (ch_data->pcm_blocks)
                utk_rev3_decode_frame(ctx);
            else
                utk_decode_frame(ctx);

            ch_data->samples_used = 0;
            ch_data->samples_filled = 432;
        }
    }
}

static void flush_ea_mt_offsets(VGMSTREAM* vgmstream, int is_start, int samples_discard) {
    ea_mt_codec_data* data = vgmstream->codec_data;
    int i;

    if (!data) return;


    /* EA-MT frames are VBR (not byte-aligned?), so utk_decoder reads new buffer data automatically.
     * When decoding starts or a SCHl block changes, flush_ea_mt must be called to reset the state.
     * A bit hacky but would need some restructuring otherwise. */

    for (i = 0; i < vgmstream->channels; i++) {
        UTKContext* ctx = data[i].utk_context;

        data[i].streamfile = vgmstream->ch[i].streamfile; /* maybe should keep its own STREAMFILE? */
        if (is_start)
            data[i].offset = vgmstream->ch[i].channel_start_offset;
        else
            data[i].offset = vgmstream->ch[i].offset;
        utk_set_ptr(ctx, 0, 0); /* reset the buffer reader */

        if (is_start) {
            utk_reset(ctx);
            ctx->parsed_header = 0;
            data[i].samples_done = 0;
        }

        data[i].samples_filled = 0;
        data[i].samples_discard = samples_discard;
    }
}

void flush_ea_mt(VGMSTREAM* vgmstream) {
    flush_ea_mt_offsets(vgmstream, 0, 0);
}

void reset_ea_mt(VGMSTREAM* vgmstream) {
    flush_ea_mt_offsets(vgmstream, 1, 0);
}

void seek_ea_mt(VGMSTREAM* vgmstream, int32_t num_sample) {
    flush_ea_mt_offsets(vgmstream, 1, num_sample);
}

void free_ea_mt(ea_mt_codec_data* data, int channels) {
    int i;

    if (!data)
        return;

    for (i = 0; i < channels; i++) {
        free(data[i].utk_context);
    }
    free(data);
}

/* ********************** */

static size_t ea_mt_read_callback(void *dest, int size, void *arg) {
    ea_mt_codec_data *ch_data = arg;
    int bytes_read;

    bytes_read = read_streamfile(dest,ch_data->offset,size,ch_data->streamfile);
    ch_data->offset += bytes_read;

    return bytes_read;
}
