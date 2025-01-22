#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"
#include "../base/sbuf.h"


typedef struct {
    /* default */
    int samples_per_frame_d;
    int samples_this_block_d;
    /* first */
    int samples_per_frame_f;
    int samples_this_block_f;
    /* last */
    int samples_per_frame_l;
    int samples_this_block_l;

    bool has_interleave_first;
    bool has_interleave_last;
    bool has_interleave_internal_updates;
} layout_config_t;

static bool setup_helper(layout_config_t* layout, VGMSTREAM* vgmstream) {
    //TO-DO: this could be pre-calc'd after main init
    layout->has_interleave_first = vgmstream->interleave_first_block_size && vgmstream->channels > 1;
    layout->has_interleave_last = vgmstream->interleave_last_block_size && vgmstream->channels > 1;
    layout->has_interleave_internal_updates = vgmstream->codec_internal_updates;

    {
        int frame_size_d = decode_get_frame_size(vgmstream);
        layout->samples_per_frame_d = decode_get_samples_per_frame(vgmstream);
        if (frame_size_d == 0 || layout->samples_per_frame_d == 0)
            goto fail;
        layout->samples_this_block_d = vgmstream->interleave_block_size / frame_size_d * layout->samples_per_frame_d;
    }

    if (layout->has_interleave_first) {
        int frame_size_f = decode_get_frame_size(vgmstream);
        layout->samples_per_frame_f = decode_get_samples_per_frame(vgmstream); //todo samples per shortframe
        if (frame_size_f == 0 || layout->samples_per_frame_f == 0)
            goto fail;
        layout->samples_this_block_f = vgmstream->interleave_first_block_size / frame_size_f * layout->samples_per_frame_f;
    }

    if (layout->has_interleave_last) {
        int frame_size_l = decode_get_shortframe_size(vgmstream);
        layout->samples_per_frame_l = decode_get_samples_per_shortframe(vgmstream);
        if (frame_size_l == 0 || layout->samples_per_frame_l == 0) goto fail;
        layout->samples_this_block_l = vgmstream->interleave_last_block_size / frame_size_l * layout->samples_per_frame_l;
    }

    return true;
fail:
    return false;
}

static void update_default_values(layout_config_t* layout, VGMSTREAM* vgmstream, int* p_samples_per_frame, int* p_samples_this_block) {
    if (layout->has_interleave_first &&
            vgmstream->current_sample < layout->samples_this_block_f) {
        *p_samples_per_frame = layout->samples_per_frame_f;
        *p_samples_this_block = layout->samples_this_block_f;
    }
    else if (layout->has_interleave_last &&
                vgmstream->current_sample - vgmstream->samples_into_block + layout->samples_this_block_d > vgmstream->num_samples) {
        *p_samples_per_frame = layout->samples_per_frame_l;
        *p_samples_this_block = layout->samples_this_block_l;
    }
    else {
        *p_samples_per_frame = layout->samples_per_frame_d;
        *p_samples_this_block = layout->samples_this_block_d;
    }
}

static void update_loop_values(layout_config_t* layout, VGMSTREAM* vgmstream, int* p_samples_per_frame, int* p_samples_this_block) {
    if (layout->has_interleave_first &&
            vgmstream->current_sample < layout->samples_this_block_f) {
        /* use first interleave*/
        *p_samples_per_frame = layout->samples_per_frame_f;
        *p_samples_this_block = layout->samples_this_block_f;
        if (*p_samples_this_block == 0 && vgmstream->channels == 1)
            *p_samples_this_block = vgmstream->num_samples;
    }
    else if (layout->has_interleave_last) { /* assumes that won't loop back into a interleave_last */
        *p_samples_per_frame = layout->samples_per_frame_d;
        *p_samples_this_block = layout->samples_this_block_d;
        if (*p_samples_this_block == 0 && vgmstream->channels == 1)
            *p_samples_this_block = vgmstream->num_samples;
    }
}

static void update_offsets(layout_config_t* layout, VGMSTREAM* vgmstream, int* p_samples_per_frame, int* p_samples_this_block) {
    int channels = vgmstream->channels;

    if (layout->has_interleave_first &&
            vgmstream->current_sample == layout->samples_this_block_f) {
        /* interleave during first interleave: restore standard frame size after going past first interleave */
        *p_samples_per_frame = layout->samples_per_frame_d;
        *p_samples_this_block = layout->samples_this_block_d;
        if (*p_samples_this_block == 0 && channels == 1)
            *p_samples_this_block = vgmstream->num_samples;

        for (int ch = 0; ch < channels; ch++) {
            off_t skip = vgmstream->interleave_first_skip * (channels - 1 - ch) +
                            vgmstream->interleave_first_block_size * (channels - ch) +
                            vgmstream->interleave_block_size * ch;
            vgmstream->ch[ch].offset += skip;
        }
    }
    else if (layout->has_interleave_last &&
            vgmstream->current_sample + *p_samples_this_block > vgmstream->num_samples) {
        /* interleave during last interleave: adjust values again if inside last interleave */
        *p_samples_per_frame = layout->samples_per_frame_l;
        *p_samples_this_block = layout->samples_this_block_l;
        if (*p_samples_this_block == 0 && channels == 1)
            *p_samples_this_block = vgmstream->num_samples;

        for (int ch = 0; ch < channels; ch++) {
            off_t skip = vgmstream->interleave_block_size * (channels - ch) +
                            vgmstream->interleave_last_block_size * ch;
            vgmstream->ch[ch].offset += skip;
        }
    }
    else if (layout->has_interleave_internal_updates) {
        /* interleave for some decoders that have already moved offsets over their data, so skip other channels's data */
        for (int ch = 0; ch < channels; ch++) {
            off_t skip = vgmstream->interleave_block_size * (channels - 1);
            vgmstream->ch[ch].offset += skip;
        }
    }
    else {
        /* regular interleave */
        for (int ch = 0; ch < channels; ch++) {
            off_t skip = vgmstream->interleave_block_size * channels;
            vgmstream->ch[ch].offset += skip;
        }
    }

    vgmstream->samples_into_block = 0;
}


/* Decodes samples for interleaved streams.
 * Data has interleaved chunks per channel, and once one is decoded the layout moves offsets,
 * skipping other chunks (essentially a simplified variety of blocked layout).
 * Incompatible with decoders that move offsets. */
void render_vgmstream_interleave(sbuf_t* sdst, VGMSTREAM* vgmstream) {
    layout_config_t layout = {0};
    if (!setup_helper(&layout, vgmstream)) {
        VGM_LOG_ONCE("INTERLEAVE: wrong config found\n");
        sbuf_silence_rest(sdst);
        return;
    }


    /* set current values */
    int samples_per_frame, samples_this_block;
    update_default_values(&layout, vgmstream, &samples_per_frame, &samples_this_block);

    /* mono interleaved stream with no layout set, just behave like flat layout */
    if (samples_this_block == 0 && vgmstream->channels == 1)
        samples_this_block = vgmstream->num_samples;

    while (sdst->filled < sdst->samples) {

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping, restore standard interleave sizes */
            update_loop_values(&layout, vgmstream, &samples_per_frame, &samples_this_block);
            continue;
        }

        int samples_to_do = decode_get_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sdst->samples - sdst->filled)
            samples_to_do = sdst->samples - sdst->filled;

        if (samples_to_do <= 0) { /* happens when interleave is not set */
            VGM_LOG_ONCE("INTERLEAVE: wrong samples_to_do\n"); 
            goto decode_fail;
        }

        decode_vgmstream(sdst, vgmstream, samples_to_do);

        sdst->filled += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;


        /* move to next interleaved block when all samples are consumed */
        if (vgmstream->samples_into_block == samples_this_block) {
            update_offsets(&layout, vgmstream, &samples_per_frame, &samples_this_block);
        }
    }

    return;
decode_fail:
    sbuf_silence_rest(sdst);
}
