#ifndef __PLUGIN__
#define __PLUGIN__

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

class VgmstreamPlugin : public InputPlugin {
public:
    //static const char *const exts[];
    static const char *const defaults[];
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("vgmstream Decoder"), N_("vgmstream"), about, &prefs,
    };

    // accepted exts are validated at runtime in is_our_file now, this is to set a static list
    //static constexpr auto iinfo = InputInfo().with_exts(exts);
    //constexpr VgmstreamPlugin() : InputPlugin(info, iinfo) {}
    //constexpr VgmstreamPlugin() : InputPlugin (info, InputInfo().with_exts(exts)) {}

    constexpr VgmstreamPlugin() : InputPlugin (info, NULL) {}

    bool init();
    void cleanup();
    bool is_our_file(const char *filename, VFSFile &file);
    Tuple read_tuple(const char *filename, VFSFile &file);
    bool read_tag(const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool play(const char *filename, VFSFile &file);

private:
    void seek(int seek_value, int &current_sample_pos);

};

// reminder of usage, probably no more need
//const char *const VgmstreamPlugin::exts[] = { "ext1", "ext2", ...,  NULL }


typedef struct {
    bool loop_forever;
    int loop_count;
    double fade_length;
    double fade_delay;
} Settings;

extern Settings vgmstream_cfg;

void debugMessage(const char *str);
void vgmstream_cfg_load();
void vgmstream_cfg_save();

#endif
