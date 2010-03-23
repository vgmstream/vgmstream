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
  "2dx",

  "aax",
  "acm",
  "adp",
  "adpcm",
  "ads",
  "adx",
  "afc",
  "agsc",
  "ahx",
  "aifc",
  "aix",
  "amts",
  "as4",
  "asd",
  "asf",
  "asr",
  "ass",
  "ast",
  "aud",
  "aus",

  "baka",
  "bg00",
  "bgw",
  "bh2pcm",
  "bmdx",
  "bns",
  "brstm",
  "brstmspm",

  "caf",
  "capdsp",
  "ccc",
  "cfn",
  "cnk",

  "dcs",
  "de2",
  "dsp",
  "dtk",
  "dvi",
  "dxh",

  "eam",
  "emff",
  "enth",

  "fag",
  "filp",
  "fsb",

  "gbts",
  "gca",
  "gcm",
  "gcub",
  "gcw",
  "genh",
  "gms",
  "gsb",

  "hgc1",
  "his",
  "hlwav",
  "hps",
  "hwas",

  "idsp",
  "idvi",
  "ikm",
  "ild",
  "int",
  "isd",
  "ivaud",
  "ivb",

  "joe",

  "kces",
  "kcey",
  "kraw",

  "leg",
  "logg",
  "lps",
  "lwav",

  "matx",
  "mcg",
  "mi4",
  "mib",
  "mic",
  "mihb",
  "mpdsp",
  "mss",
  "msvp",
  "mus",
  "musc",
  "musx",
  "mwv",
  "mxst",
  "myspd",

  "ndp",
  "npsf",
  "nwa",

  "omu",

  "p2bt",
  "pcm",
  "pdt",
  "pnb",
  "pos",
  "ps2stm",
  "psh",
  "psw",

  "raw",
  "rkv",
  "rnd",
  "rrds",
  "rsd",
  "rsf",
  "rstm",
  "rwar",
  "rwav",
  "rws",
  "rwsd",
  "rwx",
  "rxw",

  "sab",
  "sad",
  "sap",
  "sc",
  "sck",
  "sd9",
  "sdt",
  "seg",
  "sfl",
  "sfs",
  "sl3",
  "sli",
  "smp",
  "snd",
  "sng",
  "sns",
  "spd",
  "sps",
  "spsd",
  "spw",
  "ss2",
  "ss3",
  "ss7"
  "ssm",
  "ster",
  "stma",
  "str",
  "strm",
  "sts",
  "stx",
  "svag",
  "svs",
  "swav",
  "swd",

  "tec",
  "thp",
  "tk5",
  "tydsp",

  "um3",

  "vag",
  "vas",
  "vb",
  "vgs",
  "vig",
  "vpk",
  "vs",
  "vsf",
  "vgv",

  "waa",
  "wac",
  "wad",
  "wam",
  "wavm",
  "wb",
  "wii",
  "wp2",
  "wsd",
  "wsi",
  "wvs",

  "xa",
  "xa2",
  "xa30",
  "xmu",
  "xsf",
  "xss",
  "xvas",
  "xwav",
  "xwb",

  "ydsp",
  "ymf",

  "zsd",
  "zwdsp",
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


