#include "../vgmstream.h"
#include "meta_utils.h"


/* Allocate memory and setup a VGMSTREAM */
VGMSTREAM* alloc_metastream(meta_header_t* h) {

    if (h->sample_rate <= 0 || h->sample_rate > VGMSTREAM_MAX_SAMPLE_RATE)
        return NULL;
    if (h->num_samples <= 0 || h->num_samples > VGMSTREAM_MAX_NUM_SAMPLES)
        return NULL;

    VGMSTREAM* vgmstream = allocate_vgmstream(h->channels, h->loop_flag);
    if (!vgmstream) return NULL;

    //vgmstream->channels = h->channels;
    vgmstream->sample_rate = h->sample_rate;
    vgmstream->num_samples = h->num_samples;
    vgmstream->loop_start_sample = h->loop_start;
    vgmstream->loop_end_sample = h->loop_end;

    vgmstream->coding_type = h->coding;
    vgmstream->layout_type = h->layout;
    vgmstream->meta_type = h->meta;

    vgmstream->num_streams = h->total_subsongs;
    vgmstream->stream_size = h->stream_size;
    vgmstream->interleave_block_size = h->interleave;

    
    if (h->coding == coding_NGC_DSP && (h->sf || h->sf_head)) {
        if (h->coefs_offset || h->coefs_spacing)
            dsp_read_coefs(vgmstream, h->sf ? h->sf : h->sf_head, h->coefs_offset, h->coefs_spacing, h->big_endian);
        if (h->hists_offset || h->hists_spacing)
            dsp_read_hist (vgmstream, h->sf ? h->sf : h->sf_head, h->hists_offset, h->hists_spacing, h->big_endian);
    }

    if (h->open_stream) {
        if (!vgmstream_open_stream(vgmstream, h->sf ? h->sf : h->sf_head, h->stream_offset))
            goto fail;
    }

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

