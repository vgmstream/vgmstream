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


#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif

#define PLUGIN_NAME  "vgmstream plugin " VGMSTREAM_VERSION
#define PLUGIN_INFO  PLUGIN_NAME " (" __DATE__ ")"


#define CFG_ID "vgmstream" // ID for storing in audacious
#define MIN_BUFFER_SIZE 576

/* global state */
/*EXPORT*/ VgmstreamPlugin aud_plugin_instance;
audacious_settings_t settings;

static const char* tagfile_name = "!tags.m3u";

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
    "ignore_loop",      "FALSE",
    "loop_count",       "2.0",
    "fade_length",      "10.0",
    "fade_delay",       "0.0",
    "downmix_channels", "2",
    "exts_unknown_on",  "FALSE",
    "exts_common_on",   "FALSE",
    "tagfile_disable",   "FALSE",
    NULL
};

// N_(...) for i18n but not much point here
const char VgmstreamPlugin::about[] =
    PLUGIN_INFO "\n"
    "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm, Nicknine, Thealexbarney, CyberBotX, and many others\n"
    "\n"
    "Audacious plugin:\n"
    "- ported to Audacious 3.6 by Brandon Whitehead\n"
    "- adopted from Audacious 3 port by Thomas Eppers\n"
    "- originally written by Todd Jeffreys (http://voidpointer.org/)\n"
    "\n"
    "https://github.com/vgmstream/vgmstream/\n"
    "https://sourceforge.net/projects/vgmstream/ (original)";

/* widget config: {min, max, step} */
const PreferencesWidget VgmstreamPlugin::widgets[] = {
    WidgetLabel(N_("<b>vgmstream config</b>")),
    WidgetCheck(N_("Loop forever"), WidgetBool(settings.loop_forever)),
    WidgetCheck(N_("Ignore loop"), WidgetBool(settings.ignore_loop)),
    WidgetSpin(N_("Loop count:"), WidgetFloat(settings.loop_count), {1, 100, 1.0}),
    WidgetSpin(N_("Fade length:"), WidgetFloat(settings.fade_time), {0, 60, 0.1}),
    WidgetSpin(N_("Fade delay:"), WidgetFloat(settings.fade_delay), {0, 60, 0.1}),
    WidgetSpin(N_("Downmix:"), WidgetInt(settings.downmix_channels), {0, 8, 1}),
    WidgetCheck(N_("Enable unknown exts"), WidgetBool(settings.exts_unknown_on)),
    // Audacious 3.6 will only match one plugin so this option has no actual use
    // (ex. a fake .flac only gets to the FLAC plugin and never to vgmstream, even on error)
    //WidgetCheck(N_("Enable common exts"), WidgetBool(settings.exts_common_on)),
    WidgetCheck(N_("Disable tagfile"), WidgetBool(settings.tagfile_disable))
};

void vgmstream_settings_load() {
    AUDINFO("load settings\n");
    aud_config_set_defaults(CFG_ID, VgmstreamPlugin::defaults);
    settings.loop_forever = aud_get_bool(CFG_ID, "loop_forever");
    settings.ignore_loop = aud_get_bool(CFG_ID, "ignore_loop");
    settings.loop_count = aud_get_double(CFG_ID, "loop_count");
    settings.fade_time = aud_get_double(CFG_ID, "fade_length");
    settings.fade_delay = aud_get_double(CFG_ID, "fade_delay");
    settings.downmix_channels = aud_get_int(CFG_ID, "downmix_channels");
    settings.exts_unknown_on = aud_get_bool(CFG_ID, "exts_unknown_on");
    settings.exts_common_on = aud_get_bool(CFG_ID, "exts_common_on");
}

void vgmstream_settings_save() {
    AUDINFO("save settings\n");
    aud_set_bool(CFG_ID, "loop_forever", settings.loop_forever);
    aud_set_bool(CFG_ID, "ignore_loop", settings.ignore_loop);
    aud_set_double(CFG_ID, "loop_count", settings.loop_count);
    aud_set_double(CFG_ID, "fade_length", settings.fade_time);
    aud_set_double(CFG_ID, "fade_delay", settings.fade_delay);
    aud_set_int(CFG_ID, "downmix_channels", settings.downmix_channels);
    aud_set_bool(CFG_ID, "exts_unknown_on", settings.exts_unknown_on);
    aud_set_bool(CFG_ID, "exts_common_on", settings.exts_common_on);
}

const PluginPreferences VgmstreamPlugin::prefs = {
    {widgets}, vgmstream_settings_load, vgmstream_settings_save
};

// validate extension (thread safe)
bool VgmstreamPlugin::is_our_file(const char * filename, VFSFile & file) {
    AUDDBG("test file=%s\n", filename);

    vgmstream_ctx_valid_cfg cfg = {0};

    cfg.accept_unknown = settings.exts_unknown_on;
    cfg.accept_common = settings.exts_common_on;

    int ok = vgmstream_ctx_is_valid(filename, &cfg);
    if (!ok) {
        return false;
    }

    // just in case reject non-supported files, to avoid hijacking certain files like .vgm
    // (other plugins should have higher priority though)
    STREAMFILE* sf = open_vfs(filename);
    if (!sf) return false;

    VGMSTREAM* infostream = init_vgmstream_from_STREAMFILE(sf);
    close_streamfile(sf);
    if (!infostream) {
        return false;
    }
    close_vgmstream(infostream);

    return true;
}


/* default output in audacious is: "INFO/DEBUG plugin.cc:xxx [(fn name)]: (msg)" */
static void vgmstream_log(int level, const char* str) {
    if (level == VGM_LOG_LEVEL_DEBUG)
        AUDDBG("%s", str);
    else
        AUDINFO("%s", str);
}

// called on startup (main thread)
bool VgmstreamPlugin::init() {
    AUDINFO("vgmstream plugin start\n");

    vgmstream_settings_load();

    vgmstream_set_log_callback(VGM_LOG_LEVEL_ALL, (void*)&vgmstream_log);

    return true;
}

// called on stop (main thread)
void VgmstreamPlugin::cleanup() {
    AUDINFO("vgmstream plugin end\n");

    vgmstream_settings_save();
}

static int get_basename_subtune(const char* filename, char* buf, int buf_len, int* p_subtune) {
    int subtune;

    const char* pos = strrchr(filename, '?');
    if (!pos)
        return 0;
    if (sscanf(pos, "?%i", &subtune) != 1)
        return 0;

    if (p_subtune)
        *p_subtune = subtune;

    strncpy(buf, filename, buf_len);
    char* pos2 = strrchr(buf, '?');
    if (pos2) //removes '?'
        pos2[0] = '\0';

    return 1;
}

static void apply_config(VGMSTREAM* vgmstream, audacious_settings_t* settings) {
    vgmstream_cfg_t vcfg = {0};

    vcfg.allow_play_forever = 1;
    vcfg.play_forever = settings->loop_forever;
    vcfg.loop_count = settings->loop_count;
    vcfg.fade_time = settings->fade_time;
    vcfg.fade_delay = settings->fade_delay;
    vcfg.ignore_loop = settings->ignore_loop;

    vgmstream_apply_config(vgmstream, &vcfg);
}

// internal helper, called every time user adds a new file to playlist
static bool read_info(const char* filename, Tuple & tuple) {
    AUDINFO("read file=%s\n", filename);

    // Audacious first calls this as a regular file (use_subtune is 0). If file has subsongs,
    // you need to detect and call set_subtunes below and return. Then Audacious will call again
    // this and "play" with "filename?N" (where N=subtune, 1=first), that must be detected and handled
    // (see plugin.h)
    char basename[PATH_LIMIT]; //filename without '?'
    int subtune = 0;
    int use_subtune = get_basename_subtune(filename, basename, sizeof(basename), &subtune);

    STREAMFILE* sf = open_vfs(use_subtune ? basename : filename);
    if (!sf) return false;

    if (use_subtune)
        sf->stream_index = subtune;

    VGMSTREAM* infostream = init_vgmstream_from_STREAMFILE(sf);
    close_streamfile(sf);
    if (!infostream) {
        return false;
    }

    int total_subtunes = infostream->num_streams;

    // int was changed to short in some version, though vgmstream formats can exceed it
    if (total_subtunes > 32767)
        total_subtunes = 32767;
    // format has subsongs but Audacious didn't ask for subsong yet
    if (total_subtunes >= 1 && !use_subtune) {
        //set nullptr to leave subsong index linear (must add +1 to subtune)
        tuple.set_subtunes(total_subtunes, nullptr);

        close_vgmstream(infostream);
        return true;
    }


    apply_config(infostream, &settings);

    int output_channels = infostream->channels;
    vgmstream_mixing_autodownmix(infostream, settings.downmix_channels);
    vgmstream_mixing_enable(infostream, 0, NULL, &output_channels);

    int bitrate = get_vgmstream_average_bitrate(infostream);
    int length_samples = vgmstream_get_samples(infostream);
    int length_ms = length_samples * 1000LL / infostream->sample_rate;

    //todo: set_format may throw std::bad_alloc if output_channels isn't supported (only 2?)
    // short form, not sure if better way
    tuple.set_format("vgmstream codec", output_channels, infostream->sample_rate, bitrate);
    tuple.set_filename(filename); //used?
    tuple.set_int(Tuple::Bitrate, bitrate); //in kb/s
    tuple.set_int(Tuple::Length, length_ms);

    //todo here we could call describe_vgmstream() and get substring to add tags and stuff
    tuple.set_str(Tuple::Codec, "vgmstream codec");
    if (use_subtune) {
        tuple.set_int(Tuple::Subtune, subtune);
        tuple.set_int(Tuple::NumSubtunes, infostream->num_streams);

        char title[1024];
        vgmstream_get_title(title, sizeof(title), basename, infostream, NULL);
        tuple.set_str(Tuple::Title, title); //may be overwritten by tags
    }


    // this function is only called when files are added to playlist,
    // so to reload tags files need to readded
    if (!settings.tagfile_disable) {
        //todo improve string functions
        char tagfile_path[PATH_LIMIT];
        strcpy(tagfile_path, filename);

        char *path = strrchr(tagfile_path,'/');
        if (path != NULL) {
            path[1] = '\0';  /* includes "/", remove after that from tagfile_path */
            strcat(tagfile_path,tagfile_name);
        }
        else { /* ??? */
            strcpy(tagfile_path,tagfile_name);
        }

        STREAMFILE* sf_tags = open_vfs(tagfile_path);
        if (sf_tags != NULL) {
            VGMSTREAM_TAGS* tags;
            const char *tag_key, *tag_val;

            tags = vgmstream_tags_init(&tag_key, &tag_val);
            vgmstream_tags_reset(tags, filename);
            while (vgmstream_tags_next_tag(tags, sf_tags)) {
                // see tuple.h (ugly but other plugins do it like this)
                if (strcasecmp(tag_key, "ARTIST") == 0)
                    tuple.set_str(Tuple::Artist, tag_val);
                else if (strcasecmp(tag_key, "ALBUMARTIST") == 0)
                    tuple.set_str(Tuple::AlbumArtist, tag_val);
                else if (strcasecmp(tag_key, "TITLE") == 0)
                    tuple.set_str(Tuple::Title, tag_val);
                else if (strcasecmp(tag_key, "ALBUM") == 0)
                    tuple.set_str(Tuple::Album, tag_val);
                else if (strcasecmp(tag_key, "PERFORMER") == 0)
                    tuple.set_str(Tuple::Performer, tag_val);
                else if (strcasecmp(tag_key, "COMPOSER") == 0)
                    tuple.set_str(Tuple::Composer, tag_val);
                else if (strcasecmp(tag_key, "COMMENT") == 0)
                    tuple.set_str(Tuple::Comment, tag_val);
                else if (strcasecmp(tag_key, "GENRE") == 0)
                    tuple.set_str(Tuple::Genre, tag_val);
                else if (strcasecmp(tag_key, "TRACK") == 0)
                    tuple.set_int(Tuple::Track, atoi(tag_val));
                else if (strcasecmp(tag_key, "YEAR") == 0)
                    tuple.set_int(Tuple::Year, atoi (tag_val));
#if defined(_AUD_PLUGIN_VERSION) && _AUD_PLUGIN_VERSION >= 48 // Audacious 3.8+
                else if (strcasecmp(tag_key, "REPLAYGAIN_TRACK_GAIN") == 0)
                    tuple.set_gain(Tuple::TrackGain, Tuple::GainDivisor, tag_val);
                else if (strcasecmp(tag_key, "REPLAYGAIN_TRACK_PEAK") == 0)
                    tuple.set_gain(Tuple::TrackPeak, Tuple::PeakDivisor, tag_val);
                else if (strcasecmp(tag_key, "REPLAYGAIN_ALBUM_GAIN") == 0)
                    tuple.set_gain(Tuple::AlbumGain, Tuple::GainDivisor, tag_val);
                else if (strcasecmp(tag_key, "REPLAYGAIN_ALBUM_PEAK") == 0)
                    tuple.set_gain(Tuple::AlbumPeak, Tuple::PeakDivisor, tag_val);
#endif
            }

            vgmstream_tags_close(tags);
            close_streamfile(sf_tags);
        }
    }

    close_vgmstream(infostream);

    return true;
}

// thread safe (for Audacious <= 3.7, unused otherwise)
Tuple VgmstreamPlugin::read_tuple(const char * filename, VFSFile & file) {
    Tuple tuple;
    read_info(filename, tuple);
    return tuple;
}

// thread safe (for Audacious >= 3.8, unused otherwise)
bool VgmstreamPlugin::read_tag(const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image) {
    return read_info(filename, tuple);
}

// internal util to seek during play
static void do_seek(VGMSTREAM* vgmstream, int seek_ms, int& current_sample_pos) {
    AUDINFO("seeking\n");

    // compute from ms to samples
    int seek_sample = (long long)seek_ms * vgmstream->sample_rate / 1000L;

    seek_vgmstream(vgmstream, seek_sample);

    current_sample_pos = seek_sample;
}

// called on play (play thread)
bool VgmstreamPlugin::play(const char * filename, VFSFile & file) {
    AUDINFO("play file=%s\n", filename);

    //handle subsongs (see read_info)
    char basename[PATH_LIMIT]; //filename without '?'
    int subtune = 0;
    int use_subtune = get_basename_subtune(filename, basename, sizeof(basename), &subtune);

    STREAMFILE* sf = open_vfs(use_subtune ? basename : filename);
    if (!sf) {
        AUDERR("failed opening file %s\n", filename);
        return false;
    }

    if (use_subtune)
        sf->stream_index = subtune;

    VGMSTREAM* vgmstream = init_vgmstream_from_STREAMFILE(sf);
    close_streamfile(sf);

    if (!vgmstream) {
        AUDINFO("filename %s is not a valid format\n", filename);
        return false;
    }

    int bitrate = get_vgmstream_average_bitrate(vgmstream);
    set_stream_bitrate(bitrate);

    //todo apply config

    apply_config(vgmstream, &settings);

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

    int play_forever = vgmstream_get_play_forever(vgmstream);
    int length_samples = vgmstream_get_samples(vgmstream);
    int decode_pos_samples = 0;

    while (!check_stop()) {
        int to_do = max_buffer_samples;

        // handle seek request
        int seek_value = check_seek();
        if (seek_value >= 0) {
            do_seek(vgmstream, seek_value, decode_pos_samples);
            continue;
        }

        // check stream finished
        if (!play_forever) {
            if (decode_pos_samples >= length_samples)
                break;

            if (decode_pos_samples + to_do > length_samples)
                to_do = length_samples - decode_pos_samples;
        }

        render_vgmstream(buffer, to_do, vgmstream);

        write_audio(buffer, to_do * sizeof(short) * output_channels);
        decode_pos_samples += to_do;
    }

    AUDINFO("play finished\n");

    close_vgmstream(vgmstream);
    return true;
}
