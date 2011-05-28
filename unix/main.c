#include <audacious/util.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include "version.h"
#include "../src/vgmstream.h"
#include "gui.h"
#include "vfs.h"
#include "settings.h"

#define TM_QUIT 0
#define TM_PLAY 1
#define TM_SEEK 2

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

extern InputPlugin vgmstream_iplug;
//static CDecoder decoder;
static volatile long decode_seek;
static GThread *decode_thread;
static GCond *ctrl_cond = NULL;
static GMutex *ctrl_mutex = NULL;
static gint stream_length_samples;
static gint fade_length_samples;
SETTINGS settings;
static gint decode_pos_samples = 0;
static VGMSTREAM *vgmstream = NULL;
static gchar strPlaying[260];
static InputPlugin *vgmstream_iplist[] = { &vgmstream_iplug, NULL };
static gint loop_forever = 0;

void vgmstream_mseek(InputPlayback *data,gulong ms);

#define CLOSE_STREAM() do { \
   if (vgmstream) close_vgmstream(vgmstream); \
   vgmstream = NULL; } while (0)

SIMPLE_INPUT_PLUGIN(vgmstream,vgmstream_iplist);

#define DS_EXIT -2

void* vgmstream_play_loop(InputPlayback *playback)
{
  int16_t buffer[576*vgmstream->channels];
  long l;
  gint seek_needed_samples;
  gint samples_to_do;
  decode_seek = -1;
  playback->playing = 1;
  playback->eof = 0;
  
  decode_pos_samples = 0;

  while (playback->playing)
  {
    // ******************************************
    // Seeking
    // ******************************************
    // check thread flags, not my favorite method
    if (decode_seek == DS_EXIT)
    {
      goto exit_thread;
    }
    else if (decode_seek >= 0)
    {
      /* compute from ms to samples */
      seek_needed_samples = (long long)decode_seek * vgmstream->sample_rate / 1000L;
      if (seek_needed_samples < decode_pos_samples)
      {
	/* go back in time, reopen file */
	reset_vgmstream(vgmstream);
	decode_pos_samples = 0;
	samples_to_do = seek_needed_samples;
      }
      else if (decode_pos_samples < seek_needed_samples)
      {
	/* go forward in time */
	samples_to_do = seek_needed_samples - decode_pos_samples;
      }
      else
      {
	/* seek to where we are, how convenient */
	samples_to_do = -1;
      }
      /* do the actual seeking */
      if (samples_to_do >= 0)
      {
	while (samples_to_do > 0)
	{
	  l = min(576,samples_to_do);
	  render_vgmstream(buffer,l,vgmstream);
	  samples_to_do -= l;
	  decode_pos_samples += l;
	}
	playback->output->flush(decode_seek);
	// reset eof flag
	playback->eof = 0;
      }
      // reset decode_seek
      decode_seek = -1;
    }
    
    // ******************************************
    // Playback
    // ******************************************
    if (!playback->eof)
    {
      // read data and pass onward
      samples_to_do = min(576,stream_length_samples - (decode_pos_samples + 576));
      l = (samples_to_do * vgmstream->channels*2);
      if (!l)
      {
	playback->eof = 1;
	// will trigger on next run through
      }
      else
      {
	// ok we read stuff
	render_vgmstream(buffer,samples_to_do,vgmstream);

    // fade!
    if (vgmstream->loop_flag && fade_length_samples > 0 && !loop_forever) {
        int samples_into_fade = decode_pos_samples - (stream_length_samples - fade_length_samples);
        if (samples_into_fade + samples_to_do > 0) {
            int j,k;
            for (j=0;j<samples_to_do;j++,samples_into_fade++) {
                if (samples_into_fade > 0) {
                    double fadedness = (double)(fade_length_samples-samples_into_fade)/fade_length_samples;
                    for (k=0;k<vgmstream->channels;k++) {
                        buffer[j*vgmstream->channels+k] =
                            (short)(buffer[j*vgmstream->channels+k]*fadedness);
                    }
                }
            }
        }
    }

    // pass it on
	playback->pass_audio(playback,FMT_S16_LE,vgmstream->channels , l , buffer , &playback->playing );

	decode_pos_samples += samples_to_do;
      }
    }
    else
    {
      // at EOF
      while (playback->output->buffer_playing())
        g_usleep(10000);
      playback->playing = 0;
      // this effectively ends the loop
    }
  }
 exit_thread:
  decode_seek = -1;
  playback->playing = 0;
  decode_pos_samples = 0;
  CLOSE_STREAM();

  return 0;
}

void vgmstream_about()
{
  vgmstream_gui_about();
}

void vgmstream_configure()
{
  vgmstream_gui_configure();
}

void vgmstream_init()
{
  LoadSettings(&settings);
  ctrl_cond = g_cond_new();
  ctrl_mutex = g_mutex_new();
}

void vgmstream_destroy()
{
  g_cond_free(ctrl_cond);
  ctrl_cond = NULL;
  g_mutex_free(ctrl_mutex);
  ctrl_mutex = NULL;
}

void vgmstream_mseek(InputPlayback *data,gulong ms)
{
  if (vgmstream)
  {
    decode_seek = ms;
    data->eof = 0;
    
    while (decode_seek != -1)
      g_usleep(10000);
  }
}

void vgmstream_play(InputPlayback *context)
{
  // this is now called in a new thread context
  vgmstream = init_vgmstream_from_STREAMFILE(open_vfs(context->filename));
  if (!vgmstream || vgmstream->channels <= 0)
  {
    CLOSE_STREAM();
    goto end_thread;
  }
  // open the audio device
  if (context->output->open_audio(FMT_S16_LE,vgmstream->sample_rate,vgmstream->channels) == 0)
  {
    CLOSE_STREAM();
    goto end_thread;
  }

  /* copy file name */
  strcpy(strPlaying,context->filename);
  // set the info
  stream_length_samples = get_vgmstream_play_samples(settings.loopcount,settings.fadeseconds,settings.fadedelayseconds,vgmstream);
  if (vgmstream->loop_flag)
  {
      fade_length_samples = settings.fadeseconds * vgmstream->sample_rate;
  } else {
      fade_length_samples = -1;
  }
  gint ms = (stream_length_samples * 1000LL) / vgmstream->sample_rate;
  gint rate   = vgmstream->sample_rate * 2 * vgmstream->channels;

  Tuple * tuple = tuple_new_from_filename(context->filename);
  tuple_associate_int(tuple, FIELD_LENGTH, NULL, ms);
  tuple_associate_int(tuple, FIELD_BITRATE, NULL, rate);
  context->set_tuple(context, tuple);
  
  decode_thread = g_thread_self();
  context->set_pb_ready(context);
  vgmstream_play_loop(context);

end_thread:
  g_mutex_lock(ctrl_mutex);
  g_cond_signal(ctrl_cond);
  g_mutex_unlock(ctrl_mutex);
}

void vgmstream_stop(InputPlayback *context)
{
  if (vgmstream)
  {
    // kill thread
    decode_seek = DS_EXIT;
    // wait for it to die
    g_mutex_lock(ctrl_mutex);
    g_cond_wait(ctrl_cond, ctrl_mutex);
    g_mutex_unlock(ctrl_mutex);
    // close audio output
  }
  context->output->close_audio();
  // cleanup 
  CLOSE_STREAM();
}

void vgmstream_pause(InputPlayback *context,gshort paused)
{
  context->output->pause(paused);
}

void vgmstream_seek(InputPlayback *context,gint time)
{
  vgmstream_mseek(context,time * 1000);
}

Tuple * vgmstream_probe_for_tuple(const gchar * filename, VFSFile * file)
{
  VGMSTREAM *infostream = NULL;
  Tuple * tuple = NULL;
  long length;

  infostream = init_vgmstream_from_STREAMFILE(open_vfs(filename));
  if (!infostream)
    goto fail;

  tuple = tuple_new_from_filename (filename);

  length = get_vgmstream_play_samples(settings.loopcount,settings.fadeseconds,settings.fadedelayseconds,infostream) * 1000LL / infostream->sample_rate;
  tuple_associate_int(tuple, FIELD_LENGTH, NULL, length);

  close_vgmstream(infostream);

  return tuple;
  
fail:
    if (tuple) {
        tuple_free(tuple);
    }
    if (infostream)
    {
        close_vgmstream(infostream);
    }
    return NULL;
}

void vgmstream_file_info_box(const gchar *pFile)
{
  char msg[1024] = {0};
  VGMSTREAM *stream;
  
  if ((stream = init_vgmstream_from_STREAMFILE(open_vfs(pFile))))
  {
    describe_vgmstream(stream,msg,sizeof(msg));
    
    close_vgmstream(stream);

    audacious_info_dialog("File information",msg,"OK",FALSE,NULL,NULL);
  }
}
