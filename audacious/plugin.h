#ifndef __PLUGIN__
#define __PLUGIN__

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#ifndef AUDACIOUS_VGMSTREAM_PRIORITY
// set higher than FFmpeg but lower than common plugins that use around 3
#ifdef _AUD_PLUGIN_DEFAULT_PRIO
# define AUDACIOUS_VGMSTREAM_PRIORITY  (_AUD_PLUGIN_DEFAULT_PRIO - 1)
#else
# define AUDACIOUS_VGMSTREAM_PRIORITY  4
#endif
#endif

class VgmstreamPlugin : public InputPlugin {
public:
    static const char *const exts[];
    static const char *const defaults[];
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("vgmstream Decoder"), N_("vgmstream"), about, &prefs,
    };

    constexpr VgmstreamPlugin() : InputPlugin (info,
            InputInfo(FlagSubtunes) // allow subsongs
            .with_priority(AUDACIOUS_VGMSTREAM_PRIORITY)  // where 0=highest, 10=lowest (older) or 5 (newer)
            .with_exts(exts)) {}  // priority exts (accepted exts are still validated at runtime)

    bool init();
    void cleanup();
    bool is_our_file(const char * filename, VFSFile & file);
    Tuple read_tuple(const char * filename, VFSFile & file);
    bool read_tag(const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool play(const char * filename, VFSFile & file);

};


typedef struct {
    bool loop_forever;
    bool ignore_loop;
    double loop_count;
    double fade_time;
    double fade_delay;
    int downmix_channels;
    bool exts_unknown_on;
    bool exts_common_on;
    bool tagfile_disable;
} audacious_settings_t;

extern audacious_settings_t settings;

void vgmstream_settings_load();
void vgmstream_settings_save();

#endif
