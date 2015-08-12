#ifndef __PLUGIN__
#define __PLUGIN__

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

class VgmStreamPlugin : public InputPlugin {
public:
  static const char *const exts[];
  static const char *const defaults[];
  static const char about[];
  static const PreferencesWidget widgets[];
  static const PluginPreferences prefs;

  static constexpr PluginInfo info = {
      N_("VGMStream Decoder"), N_("vgmstream"), about, &prefs,
  };

  static constexpr auto iinfo = InputInfo().with_exts(exts);

  constexpr VgmStreamPlugin() : InputPlugin(info, iinfo) {}

  bool init();
  void cleanup();
  bool is_our_file(const char *filename, VFSFile &file) { return false; }
  Tuple read_tuple(const char *filename, VFSFile &file);
  bool play(const char *filename, VFSFile &file);

private:
  void seek(int seek_value, int &current_sample_pos);
};

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

#define AUDACIOUSVGMSTREAM_VERSION "1.2.0"

#endif
