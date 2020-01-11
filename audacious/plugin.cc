/**
 * vgmstream for Audacious
 */

#include <cstdlib>
#include <algorithm>
#include <string.h>

#if DEBUG
#include <ctime>
#include <sys/time.h>
#endif

#include <libaudcore/audio.h>


extern "C" {
#include "../src/vgmstream.h"
#include "../src/plugins.h"
}
#include "plugin.h"
#include "vfs.h"


#ifndef VERSION
#include "version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define CFG_ID "vgmstream" // ID for storing in audacious
#define MIN_BUFFER_SIZE 576

/* global state */
/*EXPORT*/ VgmstreamPlugin aud_plugin_instance;
audacious_settings settings;
VGMSTREAM *vgmstream = NULL; //todo make local?

/* Audacious will first send the file to a plugin based on this static extension list. If none
 * accepts it'll try again all plugins, ordered by priority, until one accepts the file. Problem is,
 * mpg123 plugin has higher priority and tendency to accept files that aren't even MP3. To fix this
 * we declare a few conflicting formats so we have a better chance.
 * The extension affects only this priority and in all cases file must accepted during "is_our_file".
 */
const char *const VgmstreamPlugin::exts[] = {
        "ahx","asf","awc","ckd","fsb","genh","msf","p3d","rak","scd","str","txth","xvag", nullptr
};


const char *const VgmstreamPlugin::defaults[] = {
    "loop_forever",     "FALSE",
    "loop_count",       "2", //maybe double?
    "fade_length",      "10.0",
    "fade_delay",       "0.0",
    "downmix_channels", "8",
    "exts_unknown_on",  "FALSE",
    "exts_common_on",   "FALSE",
    NULL
};

// N_(...) for i18n but not much point here
const char VgmstreamPlugin::about[] =
    "vgmstream plugin " VERSION " " __DATE__ "\n"
    "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm and many others\n"
    "\n"
    "Audacious plugin:\n"
    "ported to Audacious 3.6 by Brandon Whitehead\n"
    "adopted from Audacious 3 port by Thomas Eppers\n"
    "originally written by Todd Jeffreys (http://voidpointer.org/)\n"
    "\n"
    "https://github.com/kode54/vgmstream/\n"
    "https://sourceforge.net/projects/vgmstream/ (original)";

const PreferencesWidget VgmstreamPlugin::widgets[] = {
    WidgetLabel(N_("<b>vgmstream config</b>")),
    WidgetCheck(N_("Loop forever:"), WidgetBool(settings.loop_forever)),
    WidgetSpin(N_("Loop count:"), WidgetInt(settings.loop_count), {1, 20, 1}),
    WidgetSpin(N_("Fade length:"), WidgetFloat(settings.fade_length), {0, 60, 0.1}),
    WidgetSpin(N_("Fade delay:"), WidgetFloat(settings.fade_delay), {0, 60, 0.1}),
    WidgetSpin(N_("Downmix:"), WidgetInt(settings.downmix_channels), {1, 20, 1}),
    WidgetCheck(N_("Enable unknown exts"), WidgetBool(settings.exts_unknown_on)),
    // Audacious 3.6 will only match one plugin so this option has no actual use
    // (ex. a fake .flac only gets to the FLAC plugin and never to vgmstream, even on error)
    //WidgetCheck(N_("Enable common exts"), WidgetBool(settings.exts_common_on)),
};

void vgmstream_settings_load() {
    AUDINFO("load settings\n");
    aud_config_set_defaults(CFG_ID, VgmstreamPlugin::defaults);
    settings.loop_forever = aud_get_bool(CFG_ID, "loop_forever");
    settings.loop_count = aud_get_int(CFG_ID, "loop_count");
    settings.fade_length = aud_get_double(CFG_ID, "fade_length");
    settings.fade_delay = aud_get_double(CFG_ID, "fade_delay");
    settings.downmix_channels = aud_get_int(CFG_ID, "downmix_channels");
    settings.exts_unknown_on = aud_get_bool(CFG_ID, "exts_unknown_on");
    settings.exts_common_on = aud_get_bool(CFG_ID, "exts_common_on");
}

void vgmstream_settings_save() {
    AUDINFO("save settings\n");
    aud_set_bool(CFG_ID, "loop_forever", settings.loop_forever);
    aud_set_int(CFG_ID, "loop_count", settings.loop_count);
    aud_set_double(CFG_ID, "fade_length", settings.fade_length);
    aud_set_double(CFG_ID, "fade_delay", settings.fade_delay);
    aud_set_int(CFG_ID, "downmix_channels", settings.downmix_channels);
    aud_set_bool(CFG_ID, "exts_unknown_on", settings.exts_unknown_on);
    aud_set_bool(CFG_ID, "exts_common_on", settings.exts_common_on);
}

const PluginPreferences VgmstreamPlugin::prefs = {
    {widgets}, vgmstream_settings_load, vgmstream_settings_save
};

// validate extension (thread safe)
bool VgmstreamPlugin::is_our_file(const char *filename, VFSFile &file) {
    AUDDBG("test file=%s\n", filename);

    vgmstream_ctx_valid_cfg cfg = {0};

    cfg.accept_unknown = settings.exts_unknown_on;
    cfg.accept_common = settings.exts_common_on;
    return vgmstream_ctx_is_valid(filename, &cfg) > 0 ? true : false;
}

// called on startup (main thread)
bool VgmstreamPlugin::init() {
    AUDINFO("plugin start\n");

    vgmstream_settings_load();

    return true;
}

// called on stop (main thread)
void VgmstreamPlugin::cleanup() {
    AUDINFO("plugin end\n");

    vgmstream_settings_save();
}

#if 0
static int get_filename_subtune(const char * filename) {
    int subtune;

    int pos = strstr("?"))
    if (pos <= 0)
        return -1;
    if (sscanf(filename + pos, "%i", &subtune) != 1)
        return -1;
    return subtune;
}
#endif

// internal helper, called every time user adds a new file to playlist
static bool read_info(const char * filename, Tuple & tuple) {
    AUDINFO("read file=%s\n", filename);

#if 0
    //todo subsongs:
    // in theory just set FlagSubtunes in plugin.h, and set_subtunes below
    // Audacious will call "play" and this(?) with filename?N where N=subtune
    // and you just load subtune N, but I can't get it working
    int subtune;
    string basename;
    int subtune = get_filename_subtune(basename, &subtune);
    //must use basename to open streamfile
#endif

    STREAMFILE *streamfile = open_vfs(filename);
    if (!streamfile) return false;

    VGMSTREAM *infostream = init_vgmstream_from_STREAMFILE(streamfile);
    if (!infostream) {
        close_streamfile(streamfile);
        return false;
    }

#if 0
    int total_subtunes = infostream->num_streams;

    //somehow max was changed to short in recent versions, though
    // some formats can exceed this
    if (total_subtunes > 32767)
        total_subtunes = 32767;
    if (infostream->num_streams > 1 && subtune <= 0) {
        //set nullptr to leave subsong index linear (must add +1 to subtune)
        tuple.set_subtunes(total_subtunes, nullptr);
        return true; //must return?
    }

    streamfile->stream_index = (subtune + 1);
#endif


    //todo apply_config
    int output_channels = infostream->channels;
    vgmstream_mixing_autodownmix(infostream, settings.downmix_channels);
    vgmstream_mixing_enable(infostream, 0, NULL, &output_channels);

    int bitrate = get_vgmstream_average_bitrate(infostream);
    int ms = get_vgmstream_play_samples(settings.loop_count, settings.fade_length, settings.fade_delay, infostream);
    ms = ms* 1000LL / infostream->sample_rate;

    // short form, not sure if better way
    tuple.set_format("vgmstream codec", output_channels, infostream->sample_rate, bitrate);
    tuple.set_filename(filename); //used?
    tuple.set_int(Tuple::Bitrate, bitrate); //in kb/s
    tuple.set_int(Tuple::Length, ms);

    //todo here we could call describe_vgmstream() and get substring to add tags and stuff
    tuple.set_str(Tuple::Codec, "vgmstream codec");
    //tuple.set_int(Tuple::Subtune, subtune);
    //tuple.set_int(Tuple::NumSubtunes, subtune); //done un set_subtunes

    //todo tags (see tuple.h)
    //tuple.set_int (Tuple::Track, ...);
    //tuple.set_str (Tuple::Artist, ...);
    //tuple.set_str (Tuple::Album, ...);

    close_streamfile(streamfile);
    close_vgmstream(infostream);

    return true;
}

// thread safe (for Audacious <= 3.7, unused otherwise)
Tuple VgmstreamPlugin::read_tuple(const char *filename, VFSFile &file) {
    Tuple tuple;
    read_info(filename, tuple);
    return tuple;
}

// thread safe (for Audacious >= 3.8, unused otherwise)
bool VgmstreamPlugin::read_tag(const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image) {
    return read_info(filename, tuple);
}

// internal util to seek during play
static void seek_helper(int seek_value, int &current_sample_pos, int input_channels) {
    AUDINFO("seeking\n");

    // compute from ms to samples
    int seek_needed_samples = (long long)seek_value * vgmstream->sample_rate / 1000L;
    short buffer[MIN_BUFFER_SIZE * input_channels];
    int max_buffer_samples = MIN_BUFFER_SIZE;

    int samples_to_do = 0;
    if (seek_needed_samples < current_sample_pos) {
        // go back in time, reopen file
        AUDINFO("resetting file to seek backwards\n");
        reset_vgmstream(vgmstream);
        current_sample_pos = 0;
        samples_to_do = seek_needed_samples;
    } else if (current_sample_pos < seek_needed_samples) {
        // go forward in time
        samples_to_do = seek_needed_samples - current_sample_pos;
    }

    // do the actual seeking
    if (samples_to_do >= 0) {
        AUDINFO("rendering forward\n");

        // render till seeked sample
        while (samples_to_do > 0) {
            int seek_samples = std::min(max_buffer_samples, samples_to_do);
            current_sample_pos += seek_samples;
            samples_to_do -= seek_samples;
            render_vgmstream(buffer, seek_samples, vgmstream);
        }
    }
}

// called on play (play thread)
bool VgmstreamPlugin::play(const char *filename, VFSFile &file) {
    AUDINFO("play file=%s\n", filename);

    // just in case
    if (vgmstream)
        close_vgmstream(vgmstream);

    STREAMFILE *streamfile = open_vfs(filename);
    if (!streamfile) {
        AUDERR("failed opening file %s\n", filename);
        return false;
    }

    vgmstream = init_vgmstream_from_STREAMFILE(streamfile);
    close_streamfile(streamfile);

    if (!vgmstream) {
        AUDINFO("filename %s is not a valid format\n", filename);
        close_vgmstream(vgmstream);
        vgmstream = NULL;
        return false;
    }

    int bitrate = get_vgmstream_average_bitrate(vgmstream);
    set_stream_bitrate(bitrate);

    //todo apply config

    int input_channels = vgmstream->channels;
    int output_channels = vgmstream->channels;
    /* enable after all config but before outbuf */
    vgmstream_mixing_autodownmix(vgmstream, settings.downmix_channels);
    vgmstream_mixing_enable(vgmstream, MIN_BUFFER_SIZE, &input_channels, &output_channels);

    //FMT_S8 / FMT_S16_NE / FMT_S24_NE / FMT_S32_NE / FMT_FLOAT
    open_audio(FMT_S16_LE, vgmstream->sample_rate, output_channels);

    // play
    short buffer[MIN_BUFFER_SIZE * input_channels];
    int max_buffer_samples = MIN_BUFFER_SIZE;
    int stream_samples_amount = get_vgmstream_play_samples(
            settings.loop_count, settings.fade_length,
            settings.fade_delay, vgmstream);
    int fade_samples = settings.fade_length * vgmstream->sample_rate;
    int current_sample_pos = 0;

    while (!check_stop()) {
        int toget = max_buffer_samples;

        // handle seek request
        int seek_value = check_seek();
        if (seek_value >= 0)
            seek_helper(seek_value, current_sample_pos, input_channels);

        // check stream finished
        if (!settings.loop_forever || !vgmstream->loop_flag) {
            if (current_sample_pos >= stream_samples_amount)
                break;
            if (current_sample_pos + toget > stream_samples_amount)
                toget = stream_samples_amount - current_sample_pos;
        }

        render_vgmstream(buffer, toget, vgmstream);

        if (vgmstream->loop_flag && fade_samples > 0 &&
                !settings.loop_forever) {
            int samples_into_fade =
                    current_sample_pos - (stream_samples_amount - fade_samples);
            if (samples_into_fade + toget > 0) {
                for (int j = 0; j < toget; j++, samples_into_fade++) {
                    if (samples_into_fade > 0) {
                        double fadedness =
                                (double)(fade_samples - samples_into_fade) / fade_samples;
                        for (int k = 0; k < output_channels; k++)
                            buffer[j * output_channels + k] *= fadedness;
                    }
                }
            }
        }

        write_audio(buffer, toget * sizeof(short) * output_channels);
        current_sample_pos += toget;
    }

    AUDINFO("play finished\n");

    close_vgmstream(vgmstream);
    vgmstream = NULL;
    return true;
}
