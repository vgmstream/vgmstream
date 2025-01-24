#include "mixer_priv.h"


// TO-DO: some ops can be done with original PCM sbuf to avoid copying to the float sbuf
// when there are no actual float ops (ex. 'swap', if no ' volume' )
// Performance gain is probably fairly small, though.

void mixer_op_swap(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* dst = smix->buf;

    for (int s = 0; s < smix->filled; s++) {
        float temp_f = dst[op->ch_dst];
        dst[op->ch_dst] = dst[op->ch_src];
        dst[op->ch_src] = temp_f;

        dst += smix->channels;
    }
}

void mixer_op_add(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* dst = smix->buf;

    /* could optimize when vol == 1 to avoid one multiplication but whatevs (not common) */
    for (int s = 0; s < smix->filled; s++) {
        dst[op->ch_dst] = dst[op->ch_dst] + dst[op->ch_src] * op->vol;

        dst += smix->channels;
    }
}

void mixer_op_volume(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* dst = smix->buf;
    
    if (op->ch_dst < 0) {
        /* "all channels", most common case */
        for (int s = 0; s < smix->filled * smix->channels; s++) {
            dst[s] = dst[s] * op->vol;
        }
    }
    else {
        for (int s = 0; s < smix->filled; s++) {
            dst[op->ch_dst] = dst[op->ch_dst] * op->vol;

            dst += smix->channels;
        }
    }
}

void mixer_op_limit(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* dst = smix->buf;

    const float limiter_max = smix->fmt == SFMT_FLT ? 1.0f : 32767.0f;
    const float limiter_min = smix->fmt == SFMT_FLT ? -1.0f : -32768.0f;

    const float temp_max = limiter_max * op->vol;
    const float temp_min = limiter_min * op->vol;

    /* could optimize when vol == 1 to avoid one multiplication but whatevs (not common) */
    for (int s = 0; s < smix->filled; s++) {

        if (op->ch_dst < 0) {
            for (int ch = 0; ch < smix->channels; ch++) {
                if (dst[ch] > temp_max)
                    dst[ch] = temp_max;
                else if (dst[ch] < temp_min)
                    dst[ch] = temp_min;
            }
        }
        else {
            if (dst[op->ch_dst] > temp_max)
                dst[op->ch_dst] = temp_max;
            else if (dst[op->ch_dst] < temp_min)
                dst[op->ch_dst] = temp_min;
        }

        dst += smix->channels;
    }
}

void mixer_op_upmix(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* sbuf = smix->buf;

    int max_channels = smix->channels;
    smix->channels += 1;

    float* dst = sbuf + smix->filled * smix->channels;
    float* src = sbuf + smix->filled * max_channels;

    /* copy 'backwards' as otherwise would overwrite samples before moving them forward */
    for (int s = 0; s < smix->filled; s++) {
        dst -= smix->channels;
        src -= max_channels;

        int sbuf_ch = max_channels - 1;
        for (int ch = smix->channels - 1; ch >= 0; ch--) {
            if (ch == op->ch_dst) {
                dst[ch] = 0; // inserted as silent
            }
            else {
                dst[ch] = src[sbuf_ch]; // 'pull' channels backward
                sbuf_ch--;
            }
        }
    }
}

void mixer_op_downmix(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* src = smix->buf;
    float* dst = smix->buf;

    int max_channels = smix->channels;
    smix->channels -= 1;

    for (int s = 0; s < smix->filled; s++) {

        for (int ch = 0; ch < op->ch_dst; ch++) {
            dst[ch] = src[ch]; // copy untouched channels
        }

        for (int ch = op->ch_dst; ch < max_channels - 1; ch++) {
            dst[ch] = src[ch + 1]; // 'pull' dropped channels back
        }

        dst += smix->channels;
        src += max_channels;
    }
}

void mixer_op_killmix(mixer_t* mixer, mix_op_t* op) {
    sbuf_t* smix = &mixer->smix;
    float* src = smix->buf;
    float* dst = smix->buf;

    int max_channels = smix->channels;
    smix->channels = op->ch_dst; // clamp channels

    for (int s = 0; s < smix->filled; s++) {
        for (int ch = 0; ch < smix->channels; ch++) {
            dst[ch] = src[ch];
        }

        dst += smix->channels;
        src += max_channels;
    }
}
