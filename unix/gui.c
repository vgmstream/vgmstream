#include <audacious/util.h>
#include <audacious/configdb.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "version.h"
#include <stdio.h>
#include <stdarg.h>

static GtkWidget *about_box;

void vgmstream_gui_about()
{
  if (about_box)
  {
    gdk_window_raise(about_box->window);
    return;
  }
  
  about_box = audacious_info_dialog(
    (gchar *) "About VGMStream Decoder",
    (gchar *) "[ VGMStream Decoder ]\n\n"
    "audacious-vgmstream version: " AUDACIOUSVGMSTREAM_VERSION "\n\n"
    "audacious-vgmstream written by Todd Jeffreys (http://voidpointer.org/)\n"
    "vgmstream written by hcs (http://www.sf.net/projects/vgmstream)",
    (gchar *) "OK",
    FALSE, NULL, NULL);
  gtk_signal_connect(GTK_OBJECT(about_box), "destroy",
                     GTK_SIGNAL_FUNC(gtk_widget_destroyed), &about_box);
  
}
