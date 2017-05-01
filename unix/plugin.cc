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
#include "../src/formats.h"
#include "../src/vgmstream.h"
}
#include "plugin.h"
#include "vfs.h"


#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define CFG_ID "vgmstream" // ID for storing in audacious
#define MIN_BUFFER_SIZE 576

/* internal state */
VgmstreamPlugin aud_plugin_instance;
Settings vgmstream_cfg;
VGMSTREAM *vgmstream = NULL;


const char *const VgmstreamPlugin::defaults[] = {
    "loop_forever", "1",
    "loop_count",   "2",
    "fade_length",  "3",
    "fade_delay",   "3",
    NULL
};

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
    WidgetLabel(N_("<b>vgmstream Config</b>")),
    WidgetCheck(N_("Loop Forever:"), WidgetBool(vgmstream_cfg.loop_forever)),
    WidgetSpin(N_("Loop Count:"), WidgetInt(vgmstream_cfg.loop_count), {1, 20, 1}),
    WidgetSpin(N_("Fade Length:"), WidgetFloat(vgmstream_cfg.fade_length), {0, 60, 0.1}),
    WidgetSpin(N_("Fade Delay:"), WidgetFloat(vgmstream_cfg.fade_delay), {0, 60, 0.1}),
};

void vgmstream_cfg_load() {
    debugMessage("cfg_load called");
    aud_config_set_defaults(CFG_ID, VgmstreamPlugin::defaults);
    vgmstream_cfg.loop_forever = aud_get_bool(CFG_ID, "loop_forever");
    vgmstream_cfg.loop_count = aud_get_int(CFG_ID, "loop_count");
    vgmstream_cfg.fade_length = aud_get_double(CFG_ID, "fade_length");
    vgmstream_cfg.fade_delay = aud_get_double(CFG_ID, "fade_delay");
}

void vgmstream_cfg_save() {
    debugMessage("cfg_save called");
    aud_set_bool(CFG_ID, "loop_forever", vgmstream_cfg.loop_forever);
    aud_set_int(CFG_ID, "loop_count", vgmstream_cfg.loop_count);
    aud_set_double(CFG_ID, "fade_length", vgmstream_cfg.fade_length);
    aud_set_double(CFG_ID, "fade_delay", vgmstream_cfg.fade_delay);
}

const PluginPreferences VgmstreamPlugin::prefs = {
    {widgets}, vgmstream_cfg_load, vgmstream_cfg_save
};

// validate extension
bool VgmstreamPlugin::is_our_file(const char *filename, VFSFile &file) {
    const char * ext = strrchr(filename,'.');
    if (ext==NULL)
        ext = filename+strlen(filename); /* point to null, i.e. an empty string for the extension */
    else
        ext = ext+1; /* skip the dot */

    const char ** ext_list = vgmstream_get_formats();
    int ext_list_len = vgmstream_get_formats_length();

    for (int i=0; i < ext_list_len; i++) {
        if (!strcasecmp(ext, ext_list[i]))
            return true;
    }

    return false;
}

bool VgmstreamPlugin::init() {
    debugMessage("init");

    vgmstream_cfg_load();

    debugMessage("after load cfg");
    return true;
}

void VgmstreamPlugin::cleanup() {
    debugMessage("cleanup");

    vgmstream_cfg_save();
}

// called every time the user adds a new file to playlist
bool read_data(const char * filename, Tuple & tuple) {

    STREAMFILE *streamfile = open_vfs(filename);
    if (!streamfile) return false;

    VGMSTREAM *vgmstream = init_vgmstream_from_STREAMFILE(streamfile);
    if (!vgmstream) {
        close_streamfile(streamfile);
        return false;
    }

    tuple.set_filename(filename); //may leak string???
    int rate = get_vgmstream_average_bitrate(vgmstream);
    tuple.set_int(Tuple::Bitrate, rate);

    int ms = get_vgmstream_play_samples(vgmstream_cfg.loop_count, vgmstream_cfg.fade_length, vgmstream_cfg.fade_delay, vgmstream);
    ms = ms* 1000LL / vgmstream->sample_rate;
    tuple.set_int(Tuple::Length, ms);

    tuple.set_str(Tuple::Codec, "vgmstream codec");//doesn't show?
    // here we could call describe_vgmstream() and get substring to add tags and stuff

    close_streamfile(streamfile);
    close_vgmstream(vgmstream);

    return true;
}

//for Audacious <= 3.7 (unused otherwise)
Tuple VgmstreamPlugin::read_tuple(const char *filename, VFSFile &file) {
    debugMessage("read_tuple");

    Tuple tuple;
    read_data(filename, tuple);
    return tuple;
}

//for Audacious >= 3.8 (unused otherwise)
bool VgmstreamPlugin::read_tag(const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image) {
    debugMessage("read_tag");

    return read_data(filename, tuple);
}

bool VgmstreamPlugin::play(const char *filename, VFSFile &file) {
    debugMessage("start play");

    int current_sample_pos = 0;
    int rate;

    STREAMFILE *streamfile = open_vfs(filename);
    if (!streamfile) {
        printf("failed opening %s\n", filename);
        return false;
    }

    vgmstream = init_vgmstream_from_STREAMFILE(streamfile);
    close_streamfile(streamfile);

    if (!vgmstream || vgmstream->channels <= 0) {
        printf("Error::Channels are zero or couldn't init plugin\n");
        if (vgmstream)
            close_vgmstream(vgmstream);
        vgmstream = NULL;
        return false;
    }

    short buffer[MIN_BUFFER_SIZE * vgmstream->channels];
    int max_buffer_samples = sizeof(buffer) / sizeof(buffer[0]) / vgmstream->channels;

    int stream_samples_amount = get_vgmstream_play_samples(
            vgmstream_cfg.loop_count, vgmstream_cfg.fade_length,
            vgmstream_cfg.fade_delay, vgmstream);
    rate = get_vgmstream_average_bitrate(vgmstream);

    set_stream_bitrate(rate);
    open_audio(FMT_S16_LE, vgmstream->sample_rate, 2);

    int fade_samples = vgmstream_cfg.fade_length * vgmstream->sample_rate;
    while (!check_stop()) {
        int toget = max_buffer_samples;

        int seek_value = check_seek();
        if (seek_value > 0)
            seek(seek_value, current_sample_pos);

        // If we haven't configured the plugin to play forever
        // or the current song is not loopable.
        if (!vgmstream_cfg.loop_forever || !vgmstream->loop_flag) {
            if (current_sample_pos >= stream_samples_amount)
                break;
            if (current_sample_pos + toget > stream_samples_amount)
                toget = stream_samples_amount - current_sample_pos;
        }

        render_vgmstream(buffer, toget, vgmstream);

        if (vgmstream->loop_flag && fade_samples > 0 &&
                !vgmstream_cfg.loop_forever) {
            int samples_into_fade =
                    current_sample_pos - (stream_samples_amount - fade_samples);
            if (samples_into_fade + toget > 0) {
                for (int j = 0; j < toget; j++, samples_into_fade++) {
                    if (samples_into_fade > 0) {
                        double fadedness =
                                (double)(fade_samples - samples_into_fade) / fade_samples;
                        for (int k = 0; k < vgmstream->channels; k++)
                            buffer[j * vgmstream->channels + k] *= fadedness;
                    }
                }
            }
        }

        write_audio(buffer, toget * sizeof(short) * vgmstream->channels);
        current_sample_pos += toget;
    }

    debugMessage("finished");
    if (vgmstream)
        close_vgmstream(vgmstream);
    vgmstream = NULL;
    return true;
}

void VgmstreamPlugin::seek(int seek_value, int &current_sample_pos) {
    debugMessage("seeking");

    // compute from ms to samples
    int seek_needed_samples = (long long)seek_value * vgmstream->sample_rate / 1000L;
    short buffer[MIN_BUFFER_SIZE * vgmstream->channels];
    int max_buffer_samples = sizeof(buffer) / sizeof(buffer[0]) / vgmstream->channels;

    int samples_to_do = 0;
    if (seek_needed_samples < current_sample_pos) {
        // go back in time, reopen file
        debugMessage("reopen file to seek backward");
        reset_vgmstream(vgmstream);
        current_sample_pos = 0;
        samples_to_do = seek_needed_samples;
    } else if (current_sample_pos < seek_needed_samples) {
        // go forward in time
        samples_to_do = seek_needed_samples - current_sample_pos;
    }

    // do the actual seeking
    if (samples_to_do >= 0) {
        debugMessage("render forward");

        // render till seeked sample
        while (samples_to_do > 0) {
            int seek_samples = std::min(max_buffer_samples, samples_to_do);
            current_sample_pos += seek_samples;
            samples_to_do -= seek_samples;
            render_vgmstream(buffer, seek_samples, vgmstream);
        }
        debugMessage("after render vgmstream");
    }
}

void debugMessage(const char *str) {
#ifdef DEBUG
    timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buffer[80];
    strftime(buffer, 80, "%H:%M:%S", localtime(&curTime.tv_sec));

    char currentTime[84] = "";
    sprintf(currentTime, "%s:%d", buffer, milli);
    printf("[%s] Debug: %s\n", currentTime, str);
#endif
}
