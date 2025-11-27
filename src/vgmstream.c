#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vgmstream.h"
#include "vgmstream_init.h"
#include "layout/layout.h"
#include "coding/coding.h"
#include "base/decode.h"
#include "base/render.h"
#include "base/mixing.h"
#include "base/mixer.h"
#include "base/seek_table.h"
#include "util/sf_utils.h"


static void try_dual_file_stereo(VGMSTREAM* opened_vgmstream, STREAMFILE* sf);


/*****************************************************************************/
/* INIT/META                                                                 */
/*****************************************************************************/

/* format detection and VGMSTREAM setup, uses default parameters */
VGMSTREAM* init_vgmstream(const char* const filename) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf = open_stdio_streamfile(filename);
    if (sf) {
        vgmstream = init_vgmstream_from_STREAMFILE(sf);
        close_streamfile(sf);
    }
    return vgmstream;
}

VGMSTREAM* init_vgmstream_from_STREAMFILE(STREAMFILE* sf) {
    return detect_vgmstream_format(sf);
}


bool prepare_vgmstream(VGMSTREAM* vgmstream, STREAMFILE* sf) {

    /* fail if there is nothing/too much to play (<=0 generates empty files, >N writes GBs of garbage) */
    if (vgmstream->num_samples <= 0 || vgmstream->num_samples > VGMSTREAM_MAX_NUM_SAMPLES) {
        VGM_LOG("VGMSTREAM: wrong num_samples %i\n", vgmstream->num_samples);
        return false;
    }

    /* everything should have a reasonable sample rate */
    if (vgmstream->sample_rate < VGMSTREAM_MIN_SAMPLE_RATE || vgmstream->sample_rate > VGMSTREAM_MAX_SAMPLE_RATE) {
        VGM_LOG("VGMSTREAM: wrong sample_rate %i\n", vgmstream->sample_rate);
        return false;
    }

    /* sanify loops and remove bad metadata */
    if (vgmstream->loop_flag) {
        if (vgmstream->loop_end_sample <= vgmstream->loop_start_sample
                || vgmstream->loop_end_sample > vgmstream->num_samples
                || vgmstream->loop_start_sample < 0) {
            VGM_LOG("VGMSTREAM: wrong loops ignored (lss=%i, lse=%i, ns=%i)\n",
                    vgmstream->loop_start_sample, vgmstream->loop_end_sample, vgmstream->num_samples);
            vgmstream->loop_flag = 0;
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = 0;
        }
    }

    /* test if candidate for dual stereo */
    if (vgmstream->channels == 1 && vgmstream->allow_dual_stereo == 1) {
        try_dual_file_stereo(vgmstream, sf);
    }


#ifdef VGM_USE_FFMPEG
    /* check FFmpeg streams here, for lack of a better place */
    if (vgmstream->coding_type == coding_FFmpeg) {
        int ffmpeg_subsongs = ffmpeg_get_subsong_count(vgmstream->codec_data);
        if (ffmpeg_subsongs && !vgmstream->num_streams) {
            vgmstream->num_streams = ffmpeg_subsongs;
        }
    }
#endif

    /* some players are picky with incorrect channel layouts (also messes ups downmixing calcs) */
    if (vgmstream->channel_layout > 0) {
        int count = 0, max_ch = 32;
        for (int ch = 0; ch < max_ch; ch++) {
            int bit = (vgmstream->channel_layout >> ch) & 1;
            if (ch > 17 && bit) { // unknown past 16
                VGM_LOG("VGMSTREAM: wrong bit %i in channel_layout %x\n", ch, vgmstream->channel_layout);
                vgmstream->channel_layout = 0;
                break;
            }
            count += bit;
        }

        if (count != vgmstream->channels) {
            VGM_LOG("VGMSTREAM: ignored mismatched channel_layout %04x, uses %i vs %i channels\n", vgmstream->channel_layout, count, vgmstream->channels);
            vgmstream->channel_layout = 0;
        }
    }

    /* files can have thousands subsongs, but let's put a limit */
    if (vgmstream->num_streams < 0 || vgmstream->num_streams > VGMSTREAM_MAX_SUBSONGS) {
        VGM_LOG("VGMSTREAM: wrong num_streams (ns=%i)\n", vgmstream->num_streams);
        return false;
    }

    /* save info */
    /* stream_index 0 may be used by plugins to signal "vgmstream default" (IOW don't force to 1) */
    if (vgmstream->stream_index == 0) {
        vgmstream->stream_index = sf->stream_index;
    }

    //TODO: this should be called in setup_vgmstream sometimes, but hard to detect since it's used for other stuff
    /* clean as loops are readable metadata but loop fields may contain garbage
     * (done *after* dual stereo as it needs loop fields to match) */
    if (!vgmstream->loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = 0;
    }


    setup_vgmstream(vgmstream); /* final setup */

    return true;
}

void setup_vgmstream(VGMSTREAM* vgmstream) {

    //TODO improve cleanup (done here to handle manually added layers)

    /* sanify loops and remove bad metadata (some layouts will behave incorrectly) */
    if (vgmstream->loop_flag) {
        if (vgmstream->loop_end_sample <= vgmstream->loop_start_sample
                || vgmstream->loop_end_sample > vgmstream->num_samples
                || vgmstream->loop_start_sample < 0) {
            VGM_LOG("VGMSTREAM: wrong loops ignored (lss=%i, lse=%i, ns=%i)\n",
                    vgmstream->loop_start_sample, vgmstream->loop_end_sample, vgmstream->num_samples);
            vgmstream->loop_flag = 0;
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = 0;
        }
    }

#if 0
    //TODO: this removes loop info after disabling loops externally (this must be called), though this is not very useful
    /* clean as loops are readable metadata but loop fields may contain garbage
        * (done *after* dual stereo as it needs loop fields to match) */
    if (!vgmstream->loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = 0;
    }
#endif

    /* save start things so we can restart when seeking */
    memcpy(vgmstream->start_ch, vgmstream->ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
    memcpy(vgmstream->start_vgmstream, vgmstream, sizeof(VGMSTREAM));

    /* layout's sub-VGMSTREAM are expected to setup externally and maybe call this,
     * as they can be created using init_vgmstream or manually */
}

/* Reset a VGMSTREAM to its state at the start of playback (when a plugin seeks back to zero). */
void reset_vgmstream(VGMSTREAM* vgmstream) {

    /* reset the VGMSTREAM and channels back to their original state */
    memcpy(vgmstream, vgmstream->start_vgmstream, sizeof(VGMSTREAM));
    memcpy(vgmstream->ch, vgmstream->start_ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
    /* loop_ch is not reset here because there is a possibility of the
     * init_vgmstream_* function doing something tricky and precomputing it.
     * Otherwise hit_loop will be 0 and it will be copied over anyway when we
     * really hit the loop start. */

    decode_reset(vgmstream);

    render_reset(vgmstream);

    /* note that this does not reset the constituent STREAMFILES
     * (vgmstream->ch[N].streamfiles' internal state, like internal offset, though shouldn't matter) */
}

/* Allocate memory and setup a VGMSTREAM */
VGMSTREAM* allocate_vgmstream(int channels, int loop_flag) {
    VGMSTREAM* vgmstream;

    /* up to ~16-24 aren't too rare for multilayered files, 50+ is probably a bug */
    if (channels <= 0 || channels > VGMSTREAM_MAX_CHANNELS) {
        VGM_LOG("VGMSTREAM: error allocating %i channels\n", channels);
        return NULL;
    }

    /* VGMSTREAM's alloc'ed internals work as follows:
     * - vgmstream: main struct (config+state) modified by metas, layouts and codings as needed
     * - ch: config+state per channel, also modified by those
     * - start_vgmstream: vgmstream clone copied on init_vgmstream and restored on reset_vgmstream
     * - start_ch: ch clone copied on init_vgmstream and restored on reset_vgmstream
     * - loop_ch: ch clone copied on loop start and restored on loop end (decode_do_loop)
     * - codec/layout_data: custom state for complex codecs or layouts, handled externally
     *
     * Here we only create the basic structs to be filled, and only after init_vgmstream it
     * can be considered ready. Clones are shallow copies, in that they share alloc'ed struts
     * (like, vgmstream->ch and start_vgmstream->ch will be the same after init_vgmstream, or
     * start_vgmstream->start_vgmstream will end up pointing to itself)
     *
     * This is all a bit too brittle, so code alloc'ing or changing anything sensitive should
     * take care clones are properly synced.
     */

    /* create vgmstream + main structs (other data is 0'ed) */
    vgmstream = calloc(1, sizeof(VGMSTREAM));
    if (!vgmstream) return NULL;

    vgmstream->start_vgmstream = calloc(1, sizeof(VGMSTREAM));
    if (!vgmstream->start_vgmstream) goto fail;

    vgmstream->ch = calloc(channels, sizeof(VGMSTREAMCHANNEL));
    if (!vgmstream->ch) goto fail;

    vgmstream->start_ch = calloc(channels, sizeof(VGMSTREAMCHANNEL));
    if (!vgmstream->start_ch) goto fail;

    if (loop_flag) {
        vgmstream->loop_ch = calloc(channels, sizeof(VGMSTREAMCHANNEL));
        if (!vgmstream->loop_ch) goto fail;
    }

    vgmstream->channels = channels;
    vgmstream->loop_flag = loop_flag;

    vgmstream->mixer = mixer_init(vgmstream->channels); /* pre-init */
    if (!vgmstream->mixer) goto fail;

    vgmstream->decode_state = decode_init();
    if (!vgmstream->decode_state) goto fail;

    //TODO: improve/init later to minimize memory
    /* garbage buffer for seeking/discarding (local bufs may cause stack overflows with segments/layers)
     * in theory the bigger the better but in practice there isn't much difference. */
    vgmstream->tmpbuf_size = 1024 * 2 * channels * sizeof(float);
    vgmstream->tmpbuf = malloc(vgmstream->tmpbuf_size);
    if (!vgmstream->tmpbuf) goto fail;

    /* BEWARE: merge_vgmstream does some free'ing too */ 

    //vgmstream->stream_name_size = STREAM_NAME_SIZE;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

void close_vgmstream(VGMSTREAM* vgmstream) {
    if (!vgmstream)
        return;

    seek_table_free(vgmstream);
    vgmstream->seek_table = NULL;

    decode_free(vgmstream);
    vgmstream->codec_data = NULL;

    render_free(vgmstream);
    vgmstream->layout_data = NULL;


    /* now that the special cases have had their chance, clean up the standard items */
    for (int i = 0; i < vgmstream->channels; i++) {
        if (vgmstream->ch[i].streamfile) {
            close_streamfile(vgmstream->ch[i].streamfile);
            /* Multiple channels might have the same streamfile. Find the others
                * that are the same as this and clear them so they won't be closed again. */
            for (int j = 0; j < vgmstream->channels; j++) {
                if (i != j && vgmstream->ch[j].streamfile == vgmstream->ch[i].streamfile) {
                    vgmstream->ch[j].streamfile = NULL;
                }
            }
            vgmstream->ch[i].streamfile = NULL;
        }
    }

    mixer_free(vgmstream->mixer);
    free(vgmstream->tmpbuf);
    free(vgmstream->ch);
    free(vgmstream->start_ch);
    free(vgmstream->loop_ch);
    free(vgmstream->start_vgmstream);
    free(vgmstream);
}

void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample) {
    if (!vgmstream) return;

    /* ignore bad values (may happen with layers + TXTP loop install) */
    if (loop_flag && (loop_start_sample < 0 ||
            loop_start_sample > loop_end_sample ||
            loop_end_sample > vgmstream->num_samples))
        return;

    /* this requires a bit more messing with the VGMSTREAM than I'm comfortable with... */
    if (loop_flag && !vgmstream->loop_flag && !vgmstream->loop_ch) {
        vgmstream->loop_ch = calloc(vgmstream->channels, sizeof(VGMSTREAMCHANNEL));
        if (!vgmstream->loop_ch) loop_flag = 0; /* ??? */
    }
#if 0
    /* allow in case loop_flag is re-enabled later  */
    else if (!loop_flag && vgmstream->loop_flag) {
        free(vgmstream->loop_ch);
        vgmstream->loop_ch = NULL;
    }
#endif

    vgmstream->loop_flag = loop_flag;

    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = loop_end_sample;
    }
#if 0 /* keep metadata as it's may be shown (with 'loop disabled' info) */
    else {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = 0;
    }
#endif

    /* propagate changes to layouts that need them */
    if (vgmstream->layout_type == layout_layered) {
        layered_layout_data* data = vgmstream->layout_data;

        /* layered loops using the internal VGMSTREAMs */
        for (int i = 0; i < data->layer_count; i++) {
            if (!data->layers[i]->config_enabled) { /* only in simple mode */
                vgmstream_force_loop(data->layers[i], loop_flag, loop_start_sample, loop_end_sample);
            }
            /* layer's force_loop also calls setup_vgmstream, no need to do it here */
        }
    }

    /* segmented layout loops with standard loop start/end values and works ok */

    /* notify of new initial state */
    setup_vgmstream(vgmstream);
}

void vgmstream_set_loop_target(VGMSTREAM* vgmstream, int loop_target) {
    if (!vgmstream) return;
    if (!vgmstream->loop_flag) return;


    vgmstream->loop_target = loop_target; /* loop count must be rounded (int) as otherwise target is meaningless */

    /* propagate changes to layouts that need them */
    if (vgmstream->layout_type == layout_layered) {
        layered_layout_data *data = vgmstream->layout_data;
        for (int i = 0; i < data->layer_count; i++) {
            vgmstream_set_loop_target(data->layers[i], loop_target);
        }
    }

    /* notify of new initial state */
    setup_vgmstream(vgmstream);
}


/*******************************************************************************/
/* MISC                                                                        */
/*******************************************************************************/

static bool merge_vgmstream(VGMSTREAM* opened_vgmstream, VGMSTREAM* new_vgmstream, int dfs_pair) {
    VGMSTREAMCHANNEL* new_chans = NULL;
    VGMSTREAMCHANNEL* new_loop_chans = NULL;
    VGMSTREAMCHANNEL* new_start_chans = NULL;
    const int merged_channels = 2;

    /* checked outside but just in case */
    if (!opened_vgmstream || !new_vgmstream)
        goto fail;
    //if (opened_vgmstream->channels + new_vgmstream->channels != merged_channels || dfs_pair >= merged_channels)
    //    goto fail;

    /* build the channels */
    new_chans = calloc(merged_channels, sizeof(VGMSTREAMCHANNEL));
    if (!new_chans) goto fail;

    memcpy(&new_chans[dfs_pair],&opened_vgmstream->ch[0],sizeof(VGMSTREAMCHANNEL));
    memcpy(&new_chans[dfs_pair^1],&new_vgmstream->ch[0],sizeof(VGMSTREAMCHANNEL));

    /* loop and start will be initialized later, we just need to allocate them here */
    new_start_chans = calloc(merged_channels, sizeof(VGMSTREAMCHANNEL));
    if (!new_start_chans) {
        free(new_chans);
        goto fail;
    }

    if (opened_vgmstream->loop_ch) {
        new_loop_chans = calloc(merged_channels, sizeof(VGMSTREAMCHANNEL));
        if (!new_loop_chans) {
            free(new_chans);
            free(new_start_chans);
            goto fail;
        }
    }

    //TODO: maybe should just manually open a new vgmstream and its streamfiles and close normally
    /* not using close_vgmstream as that would close the file */

    /* remove the existing structures */
    free(opened_vgmstream->ch);
    free(new_vgmstream->ch);

    free(opened_vgmstream->start_ch);
    free(new_vgmstream->start_ch);

    if (opened_vgmstream->loop_ch) {
        free(opened_vgmstream->loop_ch);
        free(new_vgmstream->loop_ch);
    }

    /* fill in the new structures */
    opened_vgmstream->ch = new_chans;
    opened_vgmstream->start_ch = new_start_chans;
    opened_vgmstream->loop_ch = new_loop_chans;

    /* stereo! */
    opened_vgmstream->channels = merged_channels;
    if (opened_vgmstream->layout_type == layout_interleave)
        opened_vgmstream->layout_type = layout_none; /* fixes some odd cases */

    /* discard the second VGMSTREAM */
    decode_free(new_vgmstream);
    mixer_free(new_vgmstream->mixer);
    free(new_vgmstream->tmpbuf);
    free(new_vgmstream->start_vgmstream);
    free(new_vgmstream);

    mixer_update_channel(opened_vgmstream->mixer); /* notify of new channel hacked-in */

    return true;
fail:
    free(new_chans);
    free(new_loop_chans);
    free(new_start_chans);
    return false;
}

/* See if there is a second file which may be the second channel, given an already opened mono vgmstream.
 * If a suitable file is found, open it and change opened_vgmstream to a stereo vgmstream. */
static void try_dual_file_stereo(VGMSTREAM* opened_vgmstream, STREAMFILE* sf) {
    /* filename search pairs for dual file stereo */
    static const char* const dfs_pairs[][2] = {
        {"L","R"}, /* most common in .dsp and .vag */
        {"l","r"}, /* same */
        {"left","right"}, /* Freaky Flyers (GC) .adp, Velocity (PSP) .vag, Hyper Fighters (Wii) .dsp */
        {"Left","Right"}, /* Geometry Wars: Galaxies (Wii) .dsp */
        {".V0",".V1"}, /* Homura (PS2) */
        {".L",".R"}, /* Crash Nitro Racing (PS2), Gradius V (PS2) */
        {"_0.dsp","_1.dsp"}, /* Wario World (GC) */
        {".adpcm","_NxEncoderOut_.adpcm"}, /* Kill la Kill: IF (Switch) */
        {".adpcm","_2.adpcm"}, /* Desire: Remaster Version (Switch) */
    };
    char new_filename[PATH_LIMIT];
    char* extension;
    int dfs_pair = -1; /* -1=no stereo, 0=opened_vgmstream is left, 1=opened_vgmstream is right */
    VGMSTREAM* new_vgmstream = NULL;
    STREAMFILE* dual_sf = NULL;
    int dfs_pair_count, extension_len, filename_len;
    int sample_variance, loop_variance;

    if (opened_vgmstream->channels != 1)
        return;

    /* custom codec/layouts aren't designed for this (should never get here anyway) */
    if (opened_vgmstream->codec_data || opened_vgmstream->layout_data)
        return;

    //todo other layouts work but some stereo codecs do weird things
    //if (opened_vgmstream->layout != layout_none) return;

    get_streamfile_name(sf, new_filename, sizeof(new_filename));
    filename_len = strlen(new_filename);
    if (filename_len < 2)
        return;

    extension = (char *)filename_extension(new_filename);
    if (extension - new_filename >= 1 && extension[-1] == '.') /* [-1] is ok, yeah */
        extension--; /* must include "." */
    extension_len = strlen(extension);


    /* find pair from base name and modify new_filename with the opposite (tries L>R then R>L) */
    dfs_pair_count = (sizeof(dfs_pairs)/sizeof(dfs_pairs[0]));
    for (int i = 0; dfs_pair == -1 && i < dfs_pair_count; i++) {
        for (int j = 0; dfs_pair == -1 && j < 2; j++) {
            const char* this_suffix = dfs_pairs[i][j];
            const char* that_suffix = dfs_pairs[i][j^1];
            size_t this_suffix_len = strlen(dfs_pairs[i][j]);
            size_t that_suffix_len = strlen(dfs_pairs[i][j^1]);

            //;VGM_LOG("DFS: l=%s, r=%s\n", this_suffix,that_suffix);

            /* if suffix matches paste opposite suffix (+ terminator) to extension pointer, thus to new_filename */
            if (filename_len > this_suffix_len && strchr(this_suffix, '.') != NULL) { /* same suffix with extension */
                //;VGM_LOG("DFS: suf+ext %s vs %s len %i\n", new_filename, this_suffix, this_suffix_len);
                if (memcmp(new_filename + (filename_len - this_suffix_len), this_suffix, this_suffix_len) == 0) {
                    memcpy (new_filename + (filename_len - this_suffix_len), that_suffix,that_suffix_len+1);
                    dfs_pair = j;
                }
            }
            else if (filename_len - extension_len > this_suffix_len) { /* same suffix without extension */
                //;VGM_LOG("DFS: suf-ext %s vs %s len %i\n", extension - this_suffix_len, this_suffix, this_suffix_len);
                if (memcmp(extension - this_suffix_len, this_suffix,this_suffix_len) == 0) {
                    memmove(extension + that_suffix_len - this_suffix_len, extension,extension_len+1); /* move old extension to end */
                    memcpy (extension - this_suffix_len, that_suffix,that_suffix_len); /* overwrite with new suffix */
                    dfs_pair = j;
                }
            }

            if (dfs_pair != -1) {
                //VGM_LOG("DFS: try %i: %s\n", dfs_pair, new_filename);
                /* try to init other channel (new_filename now has the opposite name) */
                dual_sf = open_streamfile(sf, new_filename);
                if (!dual_sf) {
                    /* restore filename and keep trying (if found it'll break and init) */
                    dfs_pair = -1;
                    get_streamfile_name(sf, new_filename, sizeof(new_filename));
                }
            }
        }
    }

    /* filename didn't have a suitable L/R-pair name */
    if (dfs_pair == -1)
        return;
    //;VGM_LOG("DFS: match %i filename=%s\n", dfs_pair, new_filename);

    init_vgmstream_t init_vgmstream_function = get_vgmstream_format_init(opened_vgmstream->format_id);
    if (init_vgmstream_function == NULL)
        goto fail;

    new_vgmstream = init_vgmstream_function(dual_sf); /* use the init function that just worked */
    close_streamfile(dual_sf);
    if (!new_vgmstream)
        goto fail;

    /* see if we were able to open the file, and if everything matched nicely */
    if (!(new_vgmstream->channels == 1 &&
            new_vgmstream->sample_rate == opened_vgmstream->sample_rate &&
            new_vgmstream->meta_type   == opened_vgmstream->meta_type &&
            new_vgmstream->coding_type == opened_vgmstream->coding_type &&
            new_vgmstream->layout_type == opened_vgmstream->layout_type &&
            /* check even if the layout doesn't use them, because it is
             * difficult to determine when it does, and they should be zero otherwise, anyway */
            new_vgmstream->interleave_block_size == opened_vgmstream->interleave_block_size &&
            new_vgmstream->interleave_last_block_size == opened_vgmstream->interleave_last_block_size)) {
        goto fail;
    }

    /* samples/loops should match even when there is no loop, except in special cases
     * in the odd cases where values diverge, will use either L's loops or R's loops depending on which file is opened */
    if (new_vgmstream->meta_type == meta_SMPL) {
        loop_variance = -1; /* right channel doesn't have loop points so this check is ignored [Homura (PS2)] */
        sample_variance = 0;
    }
    else if (new_vgmstream->meta_type == meta_DSP_STD && new_vgmstream->sample_rate <= 24000) {
        loop_variance = 170000; /* rarely loop points are a bit apart, though usually only a few samples [Harvest Moon: Tree of Tranquility (Wii)] */
        sample_variance = opened_vgmstream->loop_flag ? 1600 : 700; /* less common but loops don't reach end */
    }
    else {
        loop_variance = 0;  /* otherwise should match exactly */
        sample_variance = 0;
    }

    {
        int ns_variance = new_vgmstream->num_samples - opened_vgmstream->num_samples;

        /* either channel may be bigger */
        if (abs(ns_variance) > sample_variance)
            goto fail;
    }

    if (loop_variance >= 0) {
        int ls_variance = new_vgmstream->loop_start_sample - opened_vgmstream->loop_start_sample;
        int le_variance = new_vgmstream->loop_end_sample - opened_vgmstream->loop_end_sample;

        if (new_vgmstream->loop_flag != opened_vgmstream->loop_flag)
            goto fail;

        /* either channel may be bigger */
        if (abs(ls_variance) > loop_variance || abs(le_variance) > loop_variance)
            goto fail;
    }

    /* We seem to have a usable, matching file. Merge in the second channel. */
    if (!merge_vgmstream(opened_vgmstream, new_vgmstream, dfs_pair))
        goto fail;

    return;
fail:
    close_vgmstream(new_vgmstream);
    return;
}


/**
 * Inits vgmstream, doing two things:
 * - sets the starting offset per channel (depending on the layout)
 * - opens its own streamfile from on a base one. One streamfile per channel may be open (to improve read/seeks).
 * Should be called in metas before returning the VGMSTREAM.
 */
bool vgmstream_open_stream(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset) {
    return vgmstream_open_stream_bf(vgmstream, sf, start_offset, 0);
}

bool vgmstream_open_stream_bf(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset, bool force_multibuffer) {
    STREAMFILE* file = NULL;
    char filename[PATH_LIMIT];
    bool use_streamfile_per_channel = false;
    bool use_same_offset_per_channel = false;
    bool is_stereo_codec = false;


    if (vgmstream == NULL) {
        VGM_LOG("VGMSTREAM: buggy code (null vgmstream)\n");
        goto fail;
    }

    /* no need to open anything */
    if (vgmstream->coding_type == coding_SILENCE)
        return true;

    /* stream/offsets not needed, managed by layout */
    if (vgmstream->layout_type == layout_segmented ||
        vgmstream->layout_type == layout_layered)
        return true;

    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_NWA ||
        vgmstream->coding_type == coding_ACM ||
        vgmstream->coding_type == coding_CRI_HCA)
        return true;

#ifdef VGM_USE_VORBIS
    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_OGG_VORBIS)
        return true;
#endif

#ifdef VGM_USE_FFMPEG
    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_FFmpeg)
        return true;
#endif

    if ((vgmstream->coding_type == coding_CRI_ADX ||
            vgmstream->coding_type == coding_CRI_ADX_enc_8 ||
            vgmstream->coding_type == coding_CRI_ADX_enc_9 ||
            vgmstream->coding_type == coding_CRI_ADX_exp ||
            vgmstream->coding_type == coding_CRI_ADX_fixed) &&
            (vgmstream->interleave_block_size == 0 || vgmstream->interleave_block_size > 0x12)) {
        VGM_LOG("VGMSTREAM: ADX decoder with wrong frame size %x\n", vgmstream->interleave_block_size);
        goto fail;
    }

    if ((vgmstream->coding_type == coding_MSADPCM || vgmstream->coding_type == coding_MSADPCM_ck ||
            vgmstream->coding_type == coding_MSADPCM_mono ||
            vgmstream->coding_type == coding_MS_IMA || vgmstream->coding_type == coding_MS_IMA_mono ||
            vgmstream->coding_type == coding_PSX_cfg || vgmstream->coding_type == coding_PSX_pivotal
            ) &&
            vgmstream->frame_size == 0) {
        vgmstream->frame_size = vgmstream->interleave_block_size;
    }

    if ((vgmstream->coding_type == coding_PSX_cfg ||
            vgmstream->coding_type == coding_PSX_pivotal) &&
            (vgmstream->frame_size == 0 || vgmstream->frame_size > 0x50)) {
        VGM_LOG("VGMSTREAM: PSX-cfg decoder with wrong frame size %x\n", vgmstream->frame_size);
        goto fail;
    }

    if ((vgmstream->coding_type == coding_MSADPCM ||
            vgmstream->coding_type == coding_MSADPCM_ck ||
            vgmstream->coding_type == coding_MSADPCM_mono) &&
            (vgmstream->frame_size == 0 || vgmstream->frame_size > MSADPCM_MAX_BLOCK_SIZE)) {
        VGM_LOG("VGMSTREAM: MSADPCM decoder with wrong frame size %x\n", vgmstream->frame_size);
        goto fail;
    }

    vgmstream->codec_internal_updates = decode_uses_internal_offset_updates(vgmstream);

    /* big interleaved values for non-interleaved data may result in incorrect behavior,
     * quick fix for now since layouts are finicky, with 'interleave' left for meta info
     * (certain layouts+codecs combos results in funny output too, should rework the whole thing) */
    if (vgmstream->layout_type == layout_interleave
            && vgmstream->channels == 1
            && vgmstream->interleave_block_size > 0) {
        /* main codecs that use arbitrary interleaves but could happen for others too */
        switch(vgmstream->coding_type) {
            case coding_NGC_DSP:
            case coding_NGC_DSP_subint:
            case coding_PSX:
            case coding_PSX_badflags:
                vgmstream->interleave_block_size = 0;
                break;
            default:
                break;
        }
    }

    /* if interleave is big enough keep a buffer per channel */
    if (vgmstream->interleave_block_size * vgmstream->channels >= STREAMFILE_DEFAULT_BUFFER_SIZE) {
        use_streamfile_per_channel = true;
    }

    /* if blocked layout (implicit) use multiple streamfiles; using only one leads to
     * lots of buffer-trashing, with all the jumping around in the block layout
     * (this increases total of data read but still seems faster) */
    if (vgmstream->layout_type != layout_none && vgmstream->layout_type != layout_interleave) {
        use_streamfile_per_channel = true;
    }

    /* for hard-to-detect fixed offsets or full interleave */
    if (force_multibuffer) {
        use_streamfile_per_channel = true;
    }

    /* for mono or codecs like IMA (XBOX, MS IMA, MS ADPCM) where channels work with the same bytes */
    if (vgmstream->layout_type == layout_none) {
        use_same_offset_per_channel = true;
    }

    /* stereo codecs interleave in 2ch pairs (interleave size should still be: full_block_size / channels) */
    if (vgmstream->layout_type == layout_interleave &&
            (vgmstream->coding_type == coding_XBOX_IMA || vgmstream->coding_type == coding_MTAF)) {
        is_stereo_codec = true;
    }

    if (sf == NULL || start_offset < 0) {
        VGM_LOG("VGMSTREAM: buggy code (null streamfile / wrong start_offset)\n");
        goto fail;
    }

    get_streamfile_name(sf, filename, sizeof(filename));
    /* open the file for reading by each channel */
    {
        if (!use_streamfile_per_channel) {
            file = open_streamfile(sf, filename);
            if (!file) goto fail;
        }

        for (int ch = 0; ch < vgmstream->channels; ch++) {
            off_t offset;
            if (use_same_offset_per_channel) {
                offset = start_offset;
            }
            else if (is_stereo_codec) {
                int ch_mod = (ch & 1) ? ch - 1 : ch; // adjust odd channels (ch 0,1,2,3,4,5 > ch 0,0,2,2,4,4)
                offset = start_offset + vgmstream->interleave_block_size * ch_mod;
            }
            else if (vgmstream->interleave_first_block_size) {
                // start_offset assumes + vgmstream->interleave_first_block_size, maybe should do it here
                offset = start_offset + (vgmstream->interleave_first_block_size +  vgmstream->interleave_first_skip) * ch;
            }
            else {
                offset = start_offset + vgmstream->interleave_block_size * ch;
            }

            /* open new one if needed, useful to avoid jumping around when each channel data is too apart
             * (don't use when data is close as it'd make buffers read the full file multiple times) */
            if (use_streamfile_per_channel) {
                file = open_streamfile(sf,filename);
                if (!file) goto fail;
            }

            vgmstream->ch[ch].streamfile = file;
            vgmstream->ch[ch].channel_start_offset = offset;
            vgmstream->ch[ch].offset = offset;
        }
    }

    /* init first block for blocked layout (if not blocked this will do nothing) */
    block_update(start_offset, vgmstream);

    /* EA-MT decoder is a bit finicky and needs this when channel offsets change */
    if (vgmstream->coding_type == coding_EA_MT) {
        flush_ea_mt(vgmstream);
    }

    return true;

fail:
    /* open streams will be closed in close_vgmstream(), hopefully called by the meta */
    return false;
}

bool vgmstream_is_virtual_filename(const char* filename) {
    int len = strlen(filename);
    if (len < 6)
        return false;

    /* vgmstream can play .txtp files that have size 0 but point to another file with config
     * based only in the filename (ex. "file.fsb #2.txtp" plays 2nd subsong of file.fsb).
     *
     * Also, .m3u playlist can include files that don't exist, and players often allow filenames
     * pointing to nothing (since could be some protocol/url).
     *
     * Plugins can use both quirks to allow "virtual files" (.txtp) in .m3u that don't need
     * to exist but allow config. Plugins with this function if the filename is virtual,
     * and their STREAMFILEs should be modified as to ignore null FILEs and report size 0. */
    return strcmp(&filename[len-5], ".txtp") == 0;
}
