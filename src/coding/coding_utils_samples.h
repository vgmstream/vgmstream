#ifndef _CODING_UTILS_SAMPLES_
#define _CODING_UTILS_SAMPLES_

/* sample helpers */
//TODO maybe move to .c
// (as .h can be inlined but these probably aren't called enough times that there is a notable boost)

typedef struct {
    int16_t* samples;   /* current samples (pointer is moved once consumed) */
    int filled;         /* samples left */
    int channels;       /* max channels sample buf handles */
    //TODO may be more useful with filled+consumed and not moving *samples?
} s16buf_t;

static void s16buf_silence(sample_t** p_outbuf, int32_t* p_samples_silence, int channels) {
    int samples_silence;

    samples_silence = *p_samples_silence;

    memset(*p_outbuf, 0, samples_silence * channels * sizeof(int16_t));

    *p_outbuf += samples_silence * channels;
    *p_samples_silence -= samples_silence;
}

static void s16buf_discard(sample_t** p_outbuf, s16buf_t* sbuf, int32_t* p_samples_discard) {
    int samples_discard;

    samples_discard = *p_samples_discard;
    if (samples_discard > sbuf->filled)
        samples_discard = sbuf->filled;

    /* just ignore part of samples */

    sbuf->samples += samples_discard * sbuf->channels;
    sbuf->filled -= samples_discard;

    *p_samples_discard -= samples_discard;
}

/* copy, move and mark consumed samples */
static void s16buf_consume(sample_t** p_outbuf, s16buf_t* sbuf, int32_t* p_samples_consume) {
    int samples_consume;

    samples_consume = *p_samples_consume;
    if (samples_consume > sbuf->filled)
        samples_consume = sbuf->filled;

    /* memcpy is safe when filled/samples_copy is 0 (but must pass non-NULL bufs) */
    memcpy(*p_outbuf, sbuf->samples, samples_consume * sbuf->channels * sizeof(int16_t));

    sbuf->samples += samples_consume * sbuf->channels;
    sbuf->filled -= samples_consume;

    *p_outbuf += samples_consume * sbuf->channels;
    *p_samples_consume -= samples_consume;
}


#endif /* _CODING_UTILS_SAMPLES_ */
