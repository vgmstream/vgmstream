#include "vgmstream.h"
#include "meta/adx_header.h"
#include "meta/brstm.h"
#include "layout/interleave.h"
#include "layout/nolayout.h"
#include "coding/adx_decoder.h"
#include "coding/gcdsp_decoder.h"
#include "coding/pcm_decoder.h"

/*
 * List of functions that will recognize files. These should correspond pretty
 * directly to the metadata types
 */
#define INIT_VGMSTREAM_FCNS 2
VGMSTREAM * (*init_vgmstream_fcns[INIT_VGMSTREAM_FCNS])(const char * const) = {
    init_vgmstream_adx,
    init_vgmstream_brstm,
};

/* format detection and VGMSTREAM setup */
VGMSTREAM * init_vgmstream(const char * const filename) {
    int i;

    /* try a series of formats, see which works */
    for (i=0;i<INIT_VGMSTREAM_FCNS;i++) {
        VGMSTREAM * vgmstream = (init_vgmstream_fcns[i])(filename);
        if (vgmstream) {
            /* everything should have a reasonable sample rate */
            if (!check_sample_rate(vgmstream->sample_rate)) {
                close_vgmstream(vgmstream);
                continue;
            }
            /* save start things so we can restart for seeking */
            memcpy(vgmstream->start_ch,vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
            vgmstream->start_block_offset = vgmstream->current_block_offset;
            return vgmstream;
        }
    }

    return NULL;
}

/* simply allocate memory for the VGMSTREAM and its channels */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped) {
    VGMSTREAM * vgmstream;
    VGMSTREAMCHANNEL * channels;
    VGMSTREAMCHANNEL * start_channels;
    VGMSTREAMCHANNEL * loop_channels;

    vgmstream = calloc(1,sizeof(VGMSTREAM));
    if (!vgmstream) return NULL;

    channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!channels) {
        free(vgmstream);
        return NULL;
    }
    vgmstream->ch = channels;
    vgmstream->channels = channel_count;

    start_channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!start_channels) {
        free(vgmstream);
        free(channels);
        return NULL;
    }
    vgmstream->start_ch = start_channels;

    if (looped) {
        loop_channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
        if (!loop_channels) {
            free(vgmstream);
            free(channels);
            free(start_channels);
            return NULL;
        }
        vgmstream->loop_ch = loop_channels;
    }

    vgmstream->loop_flag = looped;

    return vgmstream;
}

void close_vgmstream(VGMSTREAM * vgmstream) {
    int i;
    if (!vgmstream) return;

    for (i=0;i<vgmstream->channels;i++)
        if (vgmstream->ch[i].streamfile)
            close_streamfile(vgmstream->ch[i].streamfile);

    if (vgmstream->loop_ch) free(vgmstream->loop_ch);
    if (vgmstream->start_ch) free(vgmstream->start_ch);
    if (vgmstream->ch) free(vgmstream->ch);

    free(vgmstream);
}

int32_t get_vgmstream_play_samples(double looptimes, double fadetime, VGMSTREAM * vgmstream) {
    if (vgmstream->loop_flag) {
        return vgmstream->loop_start_sample+(vgmstream->loop_end_sample-vgmstream->loop_start_sample)*looptimes+fadetime*vgmstream->sample_rate;
    } else return vgmstream->num_samples;
}

void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_interleave:
        case layout_interleave_shortblock:
            render_vgmstream_interleave(buffer,sample_count,vgmstream);
            break;
        case layout_none:
            render_vgmstream_nolayout(buffer,sample_count,vgmstream);
            break;
    }
}

int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            return 32;
        case coding_NGC_DSP:
            return 14;
        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM8:
            return 1;
        default:
            return 0;
    }
}

int get_vgmstream_frame_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            return 18;
        case coding_NGC_DSP:
            return 8;
        case coding_PCM16LE:
        case coding_PCM16BE:
            return 2;
        case coding_PCM8:
            return 1;
        default:
            return 0;
    }
}

void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer) {
    int chan;

    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_adx(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }

            break;
        case coding_NGC_DSP:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_gcdsp(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16LE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16LE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16BE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16BE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM8:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
    }
}
