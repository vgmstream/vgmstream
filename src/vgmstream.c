#include "vgmstream.h"
#include "fmt/adx.h"
#include "fmt/brstm.h"
#include "fmt/interleave.h"

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
    }
}
