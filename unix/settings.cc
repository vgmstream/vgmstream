#include "settings.h"

#include <ctime>
#include <sys/time.h>

#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>

#include "../src/vgmstream.h"
#include "exts.h"
#include "version.h"
#include "vfs.h"

void debugMessage(const char *str)
{
#ifdef DEBUG
    timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buffer [80];
    strftime(buffer, 80, "%H:%M:%S", localtime(&curTime.tv_sec));

    char currentTime[84] = "";
    sprintf(currentTime, "%s:%d", buffer, milli);
    printf("[%s] Debug: %s\n", currentTime, str);
#endif
}

Settings vgmstream_cfg;

bool vgmstream_init();
void vgmstream_cleanup();

Tuple vgmstream_probe_for_tuple(const char *uri, VFSFile *fd);
void vgmstream_play(const char *filename, VFSFile * file);

const char vgmstream_about[] =
{
  "audacious-vgmstream version: " AUDACIOUSVGMSTREAM_VERSION "\n\n"
  "ported to audacious 3.5.1 by Brandon Whitehead\n"
  "adopted from audacious 3 port by Thomas Eppers\n"
  "originally written by Todd Jeffreys (http://voidpointer.org/)\n"
  "vgmstream written by hcs, FastElbja, manakoAT, and bxaimc (http://www.sf.net/projects/vgmstream)"
};

static const PreferencesWidget vgmstream_widgets[] = {
    WidgetLabel(N_("<b>VGMStream Config</b>")),
    WidgetCheck(N_("Loop Forever:"), WidgetBool(vgmstream_cfg.loop_forever)),
    WidgetSpin(N_("Loop Count:"), WidgetInt(vgmstream_cfg.loop_count), {1, 20, 1}),
    WidgetSpin(N_("Fade Length:"), WidgetFloat(vgmstream_cfg.fade_length), {0, 60, 0.1}),
    WidgetSpin(N_("Fade Delay:"), WidgetFloat(vgmstream_cfg.fade_delay), {0, 60, 0.1}),
};

static const PluginPreferences vgmstream_prefs = {
    {vgmstream_widgets},
    vgmstream_cfg_load,
    vgmstream_cfg_save
};


#define AUD_PLUGIN_NAME        N_("VGMStream Decoder")
#define AUD_PLUGIN_DOMAIN      "vgmstream"
#define AUD_PLUGIN_ABOUT       vgmstream_about
#define AUD_PLUGIN_INIT        vgmstream_init
#define AUD_PLUGIN_CLEANUP     vgmstream_cleanup
#define AUD_PLUGIN_PREFS       & vgmstream_prefs
#define AUD_INPUT_IS_OUR_FILE  nullptr
#define AUD_INPUT_PLAY         vgmstream_play
#define AUD_INPUT_READ_TUPLE   vgmstream_probe_for_tuple
#define AUD_INPUT_EXTS         vgmstream_exts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
