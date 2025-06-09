/**
 * vgmstream for Audacious
 */
#include <glib.h>
#include <cstdlib>
#include <algorithm>
#include <string.h>
#include <stdio.h>

#if DEBUG
#include <ctime>
#include <sys/time.h>
#endif

#include <libaudcore/audio.h>


extern "C" {
#include "../src/libvgmstream.h"
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
#define AU_PATH_LIMIT 0x4000

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

    libvgmstream_valid_t vcfg = {0};

    vcfg.accept_unknown = settings.exts_unknown_on;
    vcfg.accept_common = settings.exts_common_on;

    int ok = libvgmstream_is_valid(filename, &vcfg);
    if (!ok) {
        return false;
    }

    // just in case reject non-supported files, to avoid hijacking certain files like .vgm
    // (other plugins should have higher priority though)
    libstreamfile_t* sf = open_vfs(filename);
    if (!sf) return false;

    libvgmstream_t* infostream = libvgmstream_create(sf, 0, NULL);
    libstreamfile_close(sf);
    if (!infostream) {
        return false;
    }
    libvgmstream_free(infostream);

    return true;
}


/* default output in audacious is: "INFO/DEBUG plugin.cc:xxx [(fn name)]: (msg)" */
static void vgmstream_log(int level, const char* str) {
    if (level == LIBVGMSTREAM_LOG_LEVEL_DEBUG)
        AUDDBG("%s", str);
    else
        AUDINFO("%s", str);
}

// called on startup (main thread)
bool VgmstreamPlugin::init() {
    AUDINFO("vgmstream plugin start\n");

    vgmstream_settings_load();

    libvgmstream_set_log(LIBVGMSTREAM_LOG_LEVEL_ALL, vgmstream_log);

    return true;
}

// called on stop (main thread)
void VgmstreamPlugin::cleanup() {
    AUDINFO("vgmstream plugin end\n");

    vgmstream_settings_save();
}

static bool get_basename_subtune(const char* filename, char* buf, int buf_len, int* p_subtune) {
    int subtune;

    const char* pos = strrchr(filename, '?');
    if (!pos)
        return false;
    if (sscanf(pos, "?%i", &subtune) != 1)
        return false;

    if (p_subtune)
        *p_subtune = subtune;

    strncpy(buf, filename, buf_len);
    char* pos2 = strrchr(buf, '?');
    if (pos2) //removes '?'
        pos2[0] = '\0';

    return true;
}
static void load_vconfig(libvgmstream_config_t* vcfg, audacious_settings_t* cfg) {
    vcfg->allow_play_forever = true;
    vcfg->play_forever = cfg->loop_forever;
    vcfg->loop_count = cfg->loop_count;
    vcfg->fade_time = cfg->fade_time;
    vcfg->fade_delay = cfg->fade_delay;
    vcfg->ignore_loop = cfg->ignore_loop;
}

// internal helper, called every time user adds a new file to playlist
static bool read_info(const char* filename, Tuple & tuple) {
    AUDINFO("read file=%s\n", filename);

    // Audacious first calls this as a regular file (use_subtune is 0). If file has subsongs,
    // you need to detect and call set_subtunes below and return. Then Audacious will call again
    // this and "play" with "filename?N" (where N=subtune, 1=first), that must be detected and handled
    // (see plugin.h + audacious-plugins/src/sid)
    char basename[AU_PATH_LIMIT]; //filename without '?'
    int subtune = 0;
    bool use_subtune = get_basename_subtune(filename, basename, sizeof(basename), &subtune);
    if (!use_subtune)
        subtune = 0;

    libstreamfile_t* sf = open_vfs(use_subtune ? basename : filename);
    if (!sf) return false;

    libvgmstream_config_t vcfg = {0};
    load_vconfig(&vcfg, &settings);

    libvgmstream_t* infostream = libvgmstream_create(sf, subtune, &vcfg);
    libstreamfile_close(sf);
    if (!infostream) {
        return false;
    }


    int total_subtunes = infostream->format->subsong_count;
    // int was changed to short in some version, though vgmstream formats can exceed it
    if (total_subtunes > 32767)
        total_subtunes = 32767;

    // format has subsongs but Audacious didn't ask for subsong yet
    if (total_subtunes >= 1 && !use_subtune) {
        // pass nullptr to leave subsong index linear (must add +1 to subtune on open)
        tuple.set_subtunes(total_subtunes, nullptr); // equivalent to setting NumSubtunes

        // In Audacious 3.10+ (95e229a) subsongs only work for extensions in VgmstreamPlugin::exts, unless the "slow_probe"
        // option is set (*Settings > Advanced > Probe contents of files with no recognized file name extensions*).
        // Can't quite maintain extensions for subsongs so simply hope the option is enabled.
        // BUT! with slow_probe disabled, adding a file > removing it > adding it again unpacks subsongs (bug?).
        // Keep going if not enabled, will play first subsong normally.
        bool subtunes_enabled = aud_get_bool("slow_probe");
        if (subtunes_enabled) {
            // stop for performance reasons (could set info but would be ignored)
            libvgmstream_free(infostream);
            return true;
        }
    }


    int bitrate = infostream->format->stream_bitrate; // must be in kb/s
    int length_samples = infostream->format->play_samples;
    int length_ms = length_samples * 1000LL / infostream->format->sample_rate;

    // pass some defaults, same as manual tuple.set_x(Tuple::Thing, x) (info only, actual validations are in open_audio)
    tuple.set_format(infostream->format->codec_name, infostream->format->channels, infostream->format->sample_rate, bitrate);
    //tuple.set_filename(filename); //used?
    tuple.set_int(Tuple::Length, length_ms);
    tuple.set_str(Tuple::Comment, "vgmstream " VGMSTREAM_VERSION); //to make clearer vgmstream is actually opening the file

    { /*if (use_subtune)*/
        tuple.set_int(Tuple::Subtune, subtune);
        tuple.set_int(Tuple::NumSubtunes, total_subtunes);

        // Audacious uses URLs,  must decode back and forth (see vfs.cc too)
        gchar* hostname;
        gchar* dec_name = g_filename_from_uri(use_subtune ? basename : filename, &hostname, NULL);

        char title[1024] = {0};
        libvgmstream_title_t cfg = {
            .filename = dec_name
        };
        libvgmstream_get_title(infostream, &cfg, title, sizeof(title));
        tuple.set_str(Tuple::Title, title); //may be overwritten by tags

        g_free(hostname);
        g_free(dec_name);
    }


    // this function is only called when files are added to playlist,
    // so to reload tags files need to re-added
    if (!settings.tagfile_disable) {
        //todo improve string functions
        char tagfile_path[AU_PATH_LIMIT];
        strcpy(tagfile_path, filename);

        char* path = strrchr(tagfile_path,'/');
        if (path != NULL) {
            path[1] = '\0';  /* includes "/", remove after that from tagfile_path */
            strcat(tagfile_path,tagfile_name);
        }
        else { /* ??? */
            strcpy(tagfile_path,tagfile_name);
        }

        libstreamfile_t* sf_tags = open_vfs(tagfile_path);
        if (sf_tags != NULL) {
            libvgmstream_tags_t* tags = NULL;

            tags = libvgmstream_tags_init(sf_tags);
            libvgmstream_tags_find(tags, filename);
            while (libvgmstream_tags_next_tag(tags)) {
                const char* tag_key = tags->key;
                const char* tag_val = tags->val;

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

            libvgmstream_tags_free(tags);
            libstreamfile_close(sf_tags);
        }
    }

    libvgmstream_free(infostream);

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
static void do_seek(libvgmstream_t* vgmstream, int seek_ms) {
    AUDINFO("seeking\n");

    // compute from ms to samples
    int seek_sample = (long long)seek_ms * vgmstream->format->sample_rate / 1000L;

    libvgmstream_seek(vgmstream, seek_sample);
}

// called on play (play thread)
bool VgmstreamPlugin::play(const char * filename, VFSFile & file) {
    AUDINFO("play file=%s\n", filename);

    // handle subsongs (see read_info)
    char basename[AU_PATH_LIMIT]; //filename without '?'
    int subtune = 0;
    int use_subtune = get_basename_subtune(filename, basename, sizeof(basename), &subtune);
    if (!use_subtune)
        subtune = 0;

    libstreamfile_t* sf = open_vfs(use_subtune ? basename : filename);
    if (!sf) {
        AUDERR("failed opening file %s\n", filename);
        return false;
    }

    libvgmstream_config_t vcfg = {0};
    load_vconfig(&vcfg, &settings);

    libvgmstream_t* vgmstream = libvgmstream_create(sf, subtune, &vcfg);
    libstreamfile_close(sf);
    if (!vgmstream) {
        AUDINFO("filename %s is not a valid format\n", filename);
        return false;
    }

    set_stream_bitrate(vgmstream->format->stream_bitrate);

    //FMT_S8 / FMT_S16_NE / FMT_S24_NE / FMT_S32_NE / FMT_FLOAT
    int format;
    switch(vgmstream->format->sample_format) {
        case LIBVGMSTREAM_SFMT_FLOAT: format = FMT_FLOAT; break;
        case LIBVGMSTREAM_SFMT_PCM16: format = FMT_S16_LE; break;
        case LIBVGMSTREAM_SFMT_PCM24: format = FMT_S24_LE; break;
        case LIBVGMSTREAM_SFMT_PCM32: format = FMT_S32_LE; break;
        default:
            libvgmstream_free(vgmstream);
            return false;
    }
    open_audio(format, vgmstream->format->sample_rate, vgmstream->format->channels);

    // play
    while (!check_stop()) {
        if (vgmstream->decoder->done)
            break;

        // handle seek request
        int seek_value = check_seek();
        if (seek_value >= 0) {
            do_seek(vgmstream, seek_value);
            continue;
        }

        int err = libvgmstream_render(vgmstream);
        if (err < 0)
            break;
        write_audio(vgmstream->decoder->buf, vgmstream->decoder->buf_bytes);
    }

    AUDINFO("play finished\n");

    libvgmstream_free(vgmstream);
    return true;
}
