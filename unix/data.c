#include <audacious/util.h>

void vgmstream_init();
void vgmstream_about();
void vgmstream_configure();
void vgmstream_destroy();
gboolean vgmstream_is_our_file(gchar *pFile);
void vgmstream_play(InputPlayback *context);
void vgmstream_stop(InputPlayback *context);
void vgmstream_pause(InputPlayback *context,gshort paused);
void vgmstream_seek(InputPlayback *context,gint time);
int vgmstream_get_time(InputPlayback *context);
void vgmstream_get_song_info(gchar *pFile,gchar **title,gint *length);
void vgmstream_mseek(InputPlayback *context,gulong ms);
void vgmstream_file_info_box(gchar *pFile);

gchar *vgmstream_exts [] = {
  "adx",
  "afc",
  "agsc",
  "ast",
  "brstm",
  "brstmspm",
  "hps",
  "strm",
  "adp",
  "rsf",
  "dsp",
  "gcw",
  "ads",
  "ss2",
  "npsf",
  "rwsd",
  "xa",
  "rxw",
  "int",
  "sts",
  "svag",
  "mib",
  "mi4",
  "mpdsp",
  "mic",
  "gcm",
  "mss",
  "raw",
  "vag",
  "gms",
  "str",
  "ild",
  "pnb",
  "wavm",
  "xwav",
  "wp2",
  "sng",
  "asf",
  "eam",
  "cfn",
  "vpk",
  "genh",
  "logg",
  "sad",
  "bmdx",
  "wsi",
  "aifc",
  "aud",
  "ahx",
  "ivb",
  "amts",
  "svs",
  "pos",
  "nwa",
  "xss",
  "sl3",
  "hgc1",
  "aus",
  "rws",
  "rsd",
  "fsb",
  /* terminator */
  NULL
};


InputPlugin vgmstream_iplug = {
  .description = "VGMStream Decoder",
  .init = vgmstream_init,
  .about = vgmstream_about,
  .configure = vgmstream_configure,
  .cleanup = vgmstream_destroy,
  .is_our_file = vgmstream_is_our_file,
  .play_file = vgmstream_play,
  .stop = vgmstream_stop,
  .pause = vgmstream_pause,
  .seek = vgmstream_seek,
  .get_time = vgmstream_get_time,
  .get_song_info = vgmstream_get_song_info,
  .vfs_extensions = vgmstream_exts,
  .mseek = vgmstream_mseek,
  .file_info_box = vgmstream_file_info_box,
};


