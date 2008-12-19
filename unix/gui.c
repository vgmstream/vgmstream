#include <audacious/util.h>
#include <audacious/configdb.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "version.h"
#include "settings.h"
#include <stdio.h>
#include <stdarg.h>

extern SETTINGS settings;
static GtkWidget *about_box;
static GtkWidget *config_win;
static GtkWidget *loopcount_win;
static GtkWidget *fadeseconds_win;
static GtkWidget *fadedelayseconds_win;

void DisplayError(char *pMsg,...)
{
  GtkWidget *mbox_win,
    *mbox_vbox1,
    *mbox_vbox2,
    *mbox_frame,
    *mbox_label,
    *mbox_bbox,
    *mbox_ok;
  va_list vlist;
  char message[1024];

  va_start(vlist,pMsg);
  vsnprintf(message,sizeof(message),pMsg,vlist);
  va_end(vlist);

  mbox_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(mbox_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_signal_connect(GTK_OBJECT(mbox_win), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &mbox_win);
  gtk_window_set_title(GTK_WINDOW(mbox_win), (gchar *)"vgmstream file information");
  gtk_window_set_policy(GTK_WINDOW(mbox_win), FALSE, FALSE, FALSE);
  gtk_container_border_width(GTK_CONTAINER(mbox_win), 10);

  mbox_vbox1 = gtk_vbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(mbox_win), mbox_vbox1);

  mbox_frame = gtk_frame_new((gchar *)" vgmstream error ");
  gtk_container_set_border_width(GTK_CONTAINER(mbox_frame), 5);
  gtk_box_pack_start(GTK_BOX(mbox_vbox1), mbox_frame, FALSE, FALSE, 0);

  mbox_vbox2 = gtk_vbox_new(FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER(mbox_vbox2), 5);
  gtk_container_add(GTK_CONTAINER(mbox_frame), mbox_vbox2);

  mbox_label = gtk_label_new((gchar *)message);
  gtk_misc_set_alignment(GTK_MISC(mbox_label), 0, 0);
  gtk_label_set_justify(GTK_LABEL(mbox_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start(GTK_BOX(mbox_vbox2), mbox_label, TRUE, TRUE, 0);
  gtk_widget_show(mbox_label);

  mbox_bbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(mbox_bbox), GTK_BUTTONBOX_SPREAD);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(mbox_bbox), 5);
  gtk_box_pack_start(GTK_BOX(mbox_vbox2), mbox_bbox, FALSE, FALSE, 0);

  mbox_ok = gtk_button_new_with_label((gchar *)"OK");
  gtk_signal_connect_object(GTK_OBJECT(mbox_ok), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(mbox_win));
  GTK_WIDGET_SET_FLAGS(mbox_ok, GTK_CAN_DEFAULT);
  gtk_box_pack_start(GTK_BOX(mbox_bbox), mbox_ok, TRUE, TRUE, 0);
  gtk_widget_show(mbox_ok);
  gtk_widget_grab_default(mbox_ok);
  
  gtk_widget_show(mbox_bbox);
  gtk_widget_show(mbox_vbox2);
  gtk_widget_show(mbox_frame);
  gtk_widget_show(mbox_vbox1);
  gtk_widget_show(mbox_win);
}

static int ToInt(const char *pText,int *val)
{
  char *end;
  if (!pText) return 0;

  *val = strtol(pText,&end,10);
  if (!end || *end)
    return 0;
  
  return 1;
}

static void OnOK()
{
  SETTINGS s;
  // update my variables
  if (!ToInt(gtk_entry_get_text(GTK_ENTRY(loopcount_win)),&s.loopcount))
  {
    DisplayError("Invalid loop times entry.");
    return;
  }
  if (!ToInt(gtk_entry_get_text(GTK_ENTRY(fadeseconds_win)),&s.fadeseconds))
  {
    DisplayError("Invalid fade delay entry.");
    return;
  }
  if (!ToInt(gtk_entry_get_text(GTK_ENTRY(fadedelayseconds_win)),&s.fadedelayseconds))
  {
    DisplayError("Invalid fade length entry.");
    return;
  }

  if (SaveSettings(&s))
  {
    settings = s;		/* update local copy */
    gtk_widget_destroy(config_win);
  }
  else
  {
    DisplayError("Unable to save settings\n");
  }
}

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
    "vgmstream written by hcs, FastElbja, manakoAT (http://www.sf.net/projects/vgmstream)",
    (gchar *) "OK",
    FALSE, NULL, NULL);
  gtk_signal_connect(GTK_OBJECT(about_box), "destroy",
                     GTK_SIGNAL_FUNC(gtk_widget_destroyed), &about_box);
  
}

void vgmstream_gui_configure()
{
  GtkWidget *hbox;
  GtkWidget *tmp;
  GtkWidget *vbox;
  GtkWidget *ok;
  GtkWidget *cancel;
  GtkWidget *bbox;
  char buf[8];
  
  if (config_win)
  {
    gdk_window_raise(config_win->window);
    return;
  }

  config_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(config_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_signal_connect(GTK_OBJECT(config_win), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &config_win);
  gtk_window_set_title(GTK_WINDOW(config_win), (gchar *)"vgmstream file configuration");
  gtk_window_set_policy(GTK_WINDOW(config_win), FALSE, FALSE, FALSE);
  gtk_container_border_width(GTK_CONTAINER(config_win), 10);

  vbox = gtk_vbox_new(FALSE,5);
  // loop count
  hbox = gtk_hbox_new(FALSE,5);
  tmp = gtk_label_new("Loop count");
  gtk_box_pack_start(GTK_BOX(hbox),tmp,FALSE,FALSE,0);

  loopcount_win = gtk_entry_new_with_max_length(3);
  gtk_editable_set_editable(GTK_EDITABLE(loopcount_win),TRUE);
  sprintf(buf,"%i",settings.loopcount);
  gtk_entry_set_text(GTK_ENTRY(loopcount_win),buf);
  gtk_box_pack_start(GTK_BOX(hbox),loopcount_win,FALSE,FALSE,0);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  // fade length
  hbox = gtk_hbox_new(FALSE,5);
  tmp = gtk_label_new("Fade length");
  gtk_box_pack_start(GTK_BOX(hbox),tmp,FALSE,FALSE,0);
  
  fadeseconds_win = gtk_entry_new_with_max_length(3);
  gtk_editable_set_editable(GTK_EDITABLE(fadeseconds_win),TRUE);
  sprintf(buf,"%i",settings.fadeseconds);
  gtk_entry_set_text(GTK_ENTRY(fadeseconds_win),buf);
  gtk_box_pack_start(GTK_BOX(hbox),fadeseconds_win,FALSE,FALSE,0);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  // fade delay
  hbox = gtk_hbox_new(FALSE,5);
  tmp = gtk_label_new("Fade delay");
  gtk_box_pack_start(GTK_BOX(hbox),tmp,FALSE,FALSE,0);

  fadedelayseconds_win = gtk_entry_new_with_max_length(3);
  gtk_editable_set_editable(GTK_EDITABLE(fadedelayseconds_win),TRUE);
  sprintf(buf,"%i",settings.fadedelayseconds);
  gtk_entry_set_text(GTK_ENTRY(fadedelayseconds_win),buf);
  gtk_box_pack_start(GTK_BOX(hbox),fadedelayseconds_win,FALSE,FALSE,0);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);

  /* buttons on bottom */
  bbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox),GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox),5);
  
  ok = gtk_button_new_with_label((gchar *)"OK");
  gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(OnOK), NULL);
  GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
  gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
  gtk_widget_grab_default(ok);
  
  cancel = gtk_button_new_with_label((gchar *)"Cancel");
  gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(config_win));
  GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
  gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
  
  gtk_box_pack_start(GTK_BOX(vbox),bbox,FALSE,FALSE,0);

  gtk_container_add(GTK_CONTAINER(config_win),vbox);
  gtk_widget_show_all(config_win);

  gtk_widget_grab_default(ok);
}
