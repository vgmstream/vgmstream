#include "coding.h"

/* Decodes EA MicroTalk (speech codec) using a copied utkencode lib.
 * EA separates MT10:1 and MT5:1 (bigger frames), but apparently are the same
 * with different encoding parameters. Later revisions may have PCM blocks (rare).
 *
 * Decoder by Andrew D'Addesio: https://github.com/daddesio/utkencode
 * Info: http://wiki.niotso.org/UTK
 *
 * The following tries to follow the original code as close as possible, with minimal changes for vgmstream
 */

/* ************************************************************************************************* */
#define UTK_BUFFER_SIZE 0x4000


/* Note: This struct assumes a member alignment of 4 bytes.
** This matters when pitch_lag > 216 on the first subframe of any given frame. */
typedef struct UTKContext {
    uint8_t buffer[UTK_BUFFER_SIZE]; //vgmstream extra
    STREAMFILE * streamfile; //vgmstream extra
    unsigned int offset; //vgmstream extra
    //FILE *fp; //vgmstream extra
    const uint8_t *ptr, *end;
    int parsed_header;
    unsigned int bits_value;
    int bits_count;
    int reduced_bw;
    int multipulse_thresh;
    float fixed_gains[64];
    float rc[12];
    float synth_history[12];
    float adapt_cb[324];
    float decompressed_frame[432];
} UTKContext;

enum {
    MDL_NORMAL = 0,
    MDL_LARGEPULSE = 1
};

static const float utk_rc_table[64] = {
    +0.0f,
    -.99677598476409912109375f, -.99032700061798095703125f, -.983879029750823974609375f, -.977430999279022216796875f,
    -.970982015132904052734375f, -.964533984661102294921875f, -.958085000514984130859375f, -.9516370296478271484375f,
    -.930754005908966064453125f, -.904959976673126220703125f, -.879167020320892333984375f, -.853372991085052490234375f,
    -.827579021453857421875f, -.801786005496978759765625f, -.775991976261138916015625f, -.75019800662994384765625f,
    -.724404990673065185546875f, -.6986110210418701171875f, -.6706349849700927734375f, -.61904799938201904296875f,
    -.567460000514984130859375f, -.515873014926910400390625f, -.4642859995365142822265625f, -.4126980006694793701171875f,
    -.361110985279083251953125f, -.309523999691009521484375f, -.257937014102935791015625f, -.20634900033473968505859375f,
    -.1547619998455047607421875f, -.10317499935626983642578125f, -.05158700048923492431640625f,
    +0.0f,
    +.05158700048923492431640625f, +.10317499935626983642578125f, +.1547619998455047607421875f, +.20634900033473968505859375f,
    +.257937014102935791015625f, +.309523999691009521484375f, +.361110985279083251953125f, +.4126980006694793701171875f,
    +.4642859995365142822265625f, +.515873014926910400390625f, +.567460000514984130859375f, +.61904799938201904296875f,
    +.6706349849700927734375f, +.6986110210418701171875f, +.724404990673065185546875f, +.75019800662994384765625f,
    +.775991976261138916015625f, +.801786005496978759765625f, +.827579021453857421875f, +.853372991085052490234375f,
    +.879167020320892333984375f, +.904959976673126220703125f, +.930754005908966064453125f, +.9516370296478271484375f,
    +.958085000514984130859375f, +.964533984661102294921875f, +.970982015132904052734375f, +.977430999279022216796875f,
    +.983879029750823974609375f, +.99032700061798095703125f, +.99677598476409912109375f
};

static const uint8_t utk_codebooks[2][256] = {
    { /* normal model */
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 21,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 25,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 22,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5,  0,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 21,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 26,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 22,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5,  2
    }, { /* large-pulse model */
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 27,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  1,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 28,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  3,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 27,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  1,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 28,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  3
    }
};

static const struct {
    int next_model;
    int code_size;
    float pulse_value;
} utk_commands[29] = {
    {MDL_LARGEPULSE, 8,  0.0f},
    {MDL_LARGEPULSE, 7,  0.0f},
    {MDL_NORMAL,     8,  0.0f},
    {MDL_NORMAL,     7,  0.0f},
    {MDL_NORMAL,     2,  0.0f},
    {MDL_NORMAL,     2, -1.0f},
    {MDL_NORMAL,     2, +1.0f},
    {MDL_NORMAL,     3, -1.0f},
    {MDL_NORMAL,     3, +1.0f},
    {MDL_LARGEPULSE, 4, -2.0f},
    {MDL_LARGEPULSE, 4, +2.0f},
    {MDL_LARGEPULSE, 3, -2.0f},
    {MDL_LARGEPULSE, 3, +2.0f},
    {MDL_LARGEPULSE, 5, -3.0f},
    {MDL_LARGEPULSE, 5, +3.0f},
    {MDL_LARGEPULSE, 4, -3.0f},
    {MDL_LARGEPULSE, 4, +3.0f},
    {MDL_LARGEPULSE, 6, -4.0f},
    {MDL_LARGEPULSE, 6, +4.0f},
    {MDL_LARGEPULSE, 5, -4.0f},
    {MDL_LARGEPULSE, 5, +4.0f},
    {MDL_LARGEPULSE, 7, -5.0f},
    {MDL_LARGEPULSE, 7, +5.0f},
    {MDL_LARGEPULSE, 6, -5.0f},
    {MDL_LARGEPULSE, 6, +5.0f},
    {MDL_LARGEPULSE, 8, -6.0f},
    {MDL_LARGEPULSE, 8, +6.0f},
    {MDL_LARGEPULSE, 7, -6.0f},
    {MDL_LARGEPULSE, 7, +6.0f}
};

static int utk_read_byte(UTKContext *ctx)
{
    if (ctx->ptr < ctx->end)
        return *ctx->ptr++;

    //vgmstream extra: this reads from FILE if static buffer was exhausted, now from a context buffer and STREAMFILE instead
    if (ctx->streamfile) { //if (ctx->fp) {
        //static uint8_t buffer[4096];
        //size_t bytes_copied = fread(buffer, 1, sizeof(buffer), ctx->fp);
        size_t bytes_copied = read_streamfile(ctx->buffer, ctx->offset, sizeof(ctx->buffer), ctx->streamfile);

        ctx->offset += bytes_copied;
        if (bytes_copied > 0 && bytes_copied <= sizeof(ctx->buffer)) {
            ctx->ptr = ctx->buffer;
            ctx->end = ctx->buffer + bytes_copied;
            return *ctx->ptr++;
        }
    }

    return 0;
}

static int16_t utk_read_i16(UTKContext *ctx)
{
    int x = utk_read_byte(ctx);
    x = (x << 8) | utk_read_byte(ctx);
    return x;
}

static int utk_read_bits(UTKContext *ctx, int count)
{
    int ret = ctx->bits_value & ((1 << count) - 1);
    ctx->bits_value >>= count;
    ctx->bits_count -= count;

    if (ctx->bits_count < 8) {
        /* read another byte */
        ctx->bits_value |= utk_read_byte(ctx) << ctx->bits_count;
        ctx->bits_count += 8;
    }

    return ret;
}

static void utk_parse_header(UTKContext *ctx)
{
    int i;
    float multiplier;

    ctx->reduced_bw = utk_read_bits(ctx, 1);
    ctx->multipulse_thresh = 32 - utk_read_bits(ctx, 4);
    ctx->fixed_gains[0] = 8.0f * (1 + utk_read_bits(ctx, 4));
    multiplier = 1.04f + utk_read_bits(ctx, 6)*0.001f;

    for (i = 1; i < 64; i++)
        ctx->fixed_gains[i] = ctx->fixed_gains[i-1] * multiplier;
}

static void utk_decode_excitation(UTKContext *ctx, int use_multipulse, float *out, int stride)
{
    int i;

    if (use_multipulse) {
        /* multi-pulse model: n pulses are coded explicitly; the rest are zero */
        int model, cmd;
        model = 0;
        i = 0;
        while (i < 108) {
            cmd = utk_codebooks[model][ctx->bits_value & 0xff];
            model = utk_commands[cmd].next_model;
            utk_read_bits(ctx, utk_commands[cmd].code_size);

            if (cmd > 3) {
                /* insert a pulse with magnitude <= 6.0f */
                out[i] = utk_commands[cmd].pulse_value;
                i += stride;
            } else if (cmd > 1) {
                /* insert between 7 and 70 zeros */
                int count = 7 + utk_read_bits(ctx, 6);
                if (i + count * stride > 108)
                    count = (108 - i)/stride;

                while (count > 0) {
                    out[i] = 0.0f;
                    i += stride;
                    count--;
                }
            } else {
                /* insert a pulse with magnitude >= 7.0f */
                int x = 7;

                while (utk_read_bits(ctx, 1))
                    x++;

                if (!utk_read_bits(ctx, 1))
                    x *= -1;

                out[i] = (float)x;
                i += stride;
            }
        }
    } else {
        /* RELP model: entire residual (excitation) signal is coded explicitly */
        i = 0;
        while (i < 108) {
            if (!utk_read_bits(ctx, 1))
                out[i] = 0.0f;
            else if (!utk_read_bits(ctx, 1))
                out[i] = -2.0f;
            else
                out[i] = 2.0f;

            i += stride;
        }
    }
}

static void rc_to_lpc(const float *rc, float *lpc)
{
    int i, j;
    float tmp1[12];
    float tmp2[12];

    for (i = 10; i >= 0; i--)
        tmp2[1+i] = rc[i];

    tmp2[0] = 1.0f;

    for (i = 0; i < 12; i++) {
        float x = -tmp2[11] * rc[11];

        for (j = 10; j >= 0; j--) {
            x -= tmp2[j] * rc[j];
            tmp2[j+1] = x * rc[j] + tmp2[j];
        }

        tmp1[i] = tmp2[0] = x;

        for (j = 0; j < i; j++)
            x -= tmp1[i-1-j] * lpc[j];

        lpc[i] = x;
    }
}

static void utk_lp_synthesis_filter(UTKContext *ctx, int offset, int num_blocks)
{
    int i, j, k;
    float lpc[12];
    float *ptr = &ctx->decompressed_frame[offset];

    rc_to_lpc(ctx->rc, lpc);

    for (i = 0; i < num_blocks; i++) {
        for (j = 0; j < 12; j++) {
            float x = *ptr;

            for (k = 0; k < j; k++)
                x += lpc[k] * ctx->synth_history[k-j+12];
            for (; k < 12; k++)
                x += lpc[k] * ctx->synth_history[k-j];

            ctx->synth_history[11-j] = x;
            *ptr++ = x;
        }
    }
}

/*
** Public functions.
*/

static void utk_decode_frame(UTKContext *ctx)
{
    int i, j;
    int use_multipulse = 0;
    float excitation[5+108+5];
    float rc_delta[12];

    if (!ctx->bits_count) {
        ctx->bits_value = utk_read_byte(ctx);
        ctx->bits_count = 8;
    }

    if (!ctx->parsed_header) {
        utk_parse_header(ctx);
        ctx->parsed_header = 1;
    }

    memset(&excitation[0], 0, 5*sizeof(float));
    memset(&excitation[5+108], 0, 5*sizeof(float));

    /* read the reflection coefficients */
    for (i = 0; i < 12; i++) {
        int idx;
        if (i == 0) {
            idx = utk_read_bits(ctx, 6);
            if (idx < ctx->multipulse_thresh)
                use_multipulse = 1;
        } else if (i < 4) {
            idx = utk_read_bits(ctx, 6);
        } else {
            idx = 16 + utk_read_bits(ctx, 5);
        }

        rc_delta[i] = (utk_rc_table[idx] - ctx->rc[i])*0.25f;
    }

    /* decode four subframes */
    for (i = 0; i < 4; i++) {
        int pitch_lag = utk_read_bits(ctx, 8);
        float pitch_gain = (float)utk_read_bits(ctx, 4)/15.0f;
        float fixed_gain = ctx->fixed_gains[utk_read_bits(ctx, 6)];

        if (!ctx->reduced_bw) {
            utk_decode_excitation(ctx, use_multipulse, &excitation[5], 1);
        } else {
            /* residual (excitation) signal is encoded at reduced bandwidth */
            int align = utk_read_bits(ctx, 1);
            int zero = utk_read_bits(ctx, 1);

            utk_decode_excitation(ctx, use_multipulse, &excitation[5+align], 2);

            if (zero) {
                /* fill the remaining samples with zero
                ** (spectrum is duplicated into high frequencies) */
                for (j = 0; j < 54; j++)
                    excitation[5+(1-align)+2*j] = 0.0f;
            } else {
                /* interpolate the remaining samples
                ** (spectrum is low-pass filtered) */
                float *ptr = &excitation[5+(1-align)];
                for (j = 0; j < 108; j += 2)
                    ptr[j] =   ptr[j-5] * 0.01803267933428287506103515625f
                             - ptr[j-3] * 0.114591561257839202880859375f
                             + ptr[j-1] * 0.597385942935943603515625f
                             + ptr[j+1] * 0.597385942935943603515625f
                             - ptr[j+3] * 0.114591561257839202880859375f
                             + ptr[j+5] * 0.01803267933428287506103515625f;

                /* scale by 0.5f to give the sinc impulse response unit energy */
                fixed_gain *= 0.5f;
            }
        }

        for (j = 0; j < 108; j++)
            ctx->decompressed_frame[108*i+j] =   fixed_gain * excitation[5+j]
                                               + pitch_gain * ctx->adapt_cb[108*i+216-pitch_lag+j];
    }

    for (i = 0; i < 324; i++)
        ctx->adapt_cb[i] = ctx->decompressed_frame[108+i];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 12; j++)
            ctx->rc[j] += rc_delta[j];

        utk_lp_synthesis_filter(ctx, 12*i, i < 3 ? 1 : 33);
    }
}

static void utk_init(UTKContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

#if 0 //vgmstream extra: see flush_ea_mt
static void utk_set_fp(UTKContext *ctx, FILE *fp)
{
    ctx->fp = fp;

    /* reset the bit reader */
    ctx->bits_count = 0;
}

static void utk_set_ptr(UTKContext *ctx, const uint8_t *ptr, const uint8_t *end)
{
    ctx->ptr = ptr;
    ctx->end = end;

    /* reset the bit reader */
    ctx->bits_count = 0;
}
#endif

/*
** MicroTalk Revision 3 decoding function.
*/

static void utk_rev3_decode_frame(UTKContext *ctx)
{
    int pcm_data_present = (utk_read_byte(ctx) == 0xee);
    int i;

    utk_decode_frame(ctx);

    /* unread the last 8 bits and reset the bit reader */
    ctx->ptr--;
    ctx->bits_count = 0;

    if (pcm_data_present) {
        /* Overwrite n samples at a given offset in the decoded frame with
        ** raw PCM data. */
        int offset = utk_read_i16(ctx);
        int count = utk_read_i16(ctx);

        /* sx.exe does not do any bounds checking or clamping of these two
        ** fields (see 004274D1 in sx.exe v3.01.01), which means a specially
        ** crafted MT5:1 file can crash sx.exe.
        ** We will throw an error instead. */
        if (offset < 0 || offset > 432) {
            //fprintf(stderr, "error: invalid PCM offset %d\n", offset);
            //exit(EXIT_FAILURE);
            return; //vgmstream extra
        }
        if (count < 0 || count > 432 - offset) {
            //fprintf(stderr, "error: invalid PCM count %d\n", count);
            //exit(EXIT_FAILURE);
            return; //vgmstream extra
        }

        for (i = 0; i < count; i++)
            ctx->decompressed_frame[offset+i] = (float)utk_read_i16(ctx);
    }
}

/* ************************************************************************************************* */

//#define UTK_MAKE_U32(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define UTK_ROUND(x) ((x) >= 0.0f ? ((x)+0.5f) : ((x)-0.5f))
#define UTK_MIN(x,y) ((x)<(y)?(x):(y))
#define UTK_MAX(x,y) ((x)>(y)?(x):(y))
#define UTK_CLAMP(x,min,max) UTK_MIN(UTK_MAX(x,min),max)


ea_mt_codec_data *init_ea_mt(int channels, int pcm_blocks, int reset_sample) {
    ea_mt_codec_data *data = NULL;
    int i;

    data = calloc(channels, sizeof(ea_mt_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (i = 0; i < channels; i++) {
        data[i].utk_context = calloc(1, sizeof(UTKContext));
        if (!data[i].utk_context) goto fail;
        utk_init(data[i].utk_context);

        data[i].pcm_blocks = pcm_blocks;
        data[i].reset_sample = reset_sample;
    }

    return data;

fail:
    free_ea_mt(data, channels);
    return NULL;
}

void decode_ea_mt(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    int i;
    ea_mt_codec_data *data = vgmstream->codec_data;
    ea_mt_codec_data *ch_data = &data[channel];
    UTKContext* ctx = ch_data->utk_context;
    int samples_done = 0;


    while (samples_done < samples_to_do) {

        if (ch_data->samples_filled) {
            /* consume current frame */
            int samples_to_get = ch_data->samples_filled;

            /* don't go past loop, to reset decoder */
            if (ch_data->reset_sample > 0 && ch_data->samples_done < ch_data->reset_sample &&
                    ch_data->samples_done + samples_to_get > ch_data->reset_sample)
                samples_to_get = ch_data->reset_sample - ch_data->samples_done;

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
            if (ch_data->reset_sample > 0 && ch_data->samples_done == ch_data->reset_sample) {
                ch_data->samples_filled = 0;

                /* todo call decoder init (all fields must be reset, for some edge cases) */
                ctx->parsed_header = 0;
                ctx->bits_value = 0;
                ctx->bits_count = 0;
                ctx->reduced_bw = 0;
                ctx->multipulse_thresh = 0;
                memset(ctx->fixed_gains, 0, sizeof(ctx->fixed_gains));
                memset(ctx->rc, 0, sizeof(ctx->rc));
                memset(ctx->synth_history, 0, sizeof(ctx->synth_history));
                memset(ctx->adapt_cb, 0, sizeof(ctx->adapt_cb));
                memset(ctx->decompressed_frame, 0, sizeof(ctx->decompressed_frame));

                //todo when loop start is < 432 decoder seems to have problems
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

static void flush_ea_mt_offsets(VGMSTREAM *vgmstream, int is_start, int samples_discard) {
    ea_mt_codec_data *data = vgmstream->codec_data;
    int i;
    size_t bytes;

    if (!data) return;


    /* EA-MT frames are VBR (not byte-aligned?), so utk_decoder reads new buffer data automatically.
     * When decoding starts or a SCHl block changes, flush_ea_mt must be called to reset the state.
     * A bit hacky but would need some restructuring otherwise. */

    for (i = 0; i < vgmstream->channels; i++) {
        UTKContext* ctx = data[i].utk_context;

        ctx->streamfile = vgmstream->ch[i].streamfile; /* maybe should keep its own STREAMFILE? */
        if (is_start)
            ctx->offset = vgmstream->ch[i].channel_start_offset;
        else
            ctx->offset = vgmstream->ch[i].offset;
        //todo no need to read, allow to do it manually?
        bytes = read_streamfile(ctx->buffer,ctx->offset,sizeof(ctx->buffer),ctx->streamfile);
        ctx->offset = ctx->offset + bytes;

        ctx->ptr = ctx->buffer;
        ctx->end = ctx->buffer + bytes;
        ctx->bits_count = 0;

        if (is_start) {
            ctx->parsed_header = 0;
            data[i].samples_done = 0;
        }

        data[i].samples_filled = 0;
        data[i].samples_discard = samples_discard;
    }
}

void flush_ea_mt(VGMSTREAM *vgmstream) {
    flush_ea_mt_offsets(vgmstream, 0, 0);
}

void reset_ea_mt(VGMSTREAM *vgmstream) {
    flush_ea_mt_offsets(vgmstream, 1, 0);
}

void seek_ea_mt(VGMSTREAM * vgmstream, int32_t num_sample) {
    flush_ea_mt_offsets(vgmstream, 1, num_sample);
}

void free_ea_mt(ea_mt_codec_data *data, int channels) {
    int i;

    if (!data)
        return;

    for (i = 0; i < channels; i++) {
        free(data[i].utk_context);
    }
    free(data);
}
