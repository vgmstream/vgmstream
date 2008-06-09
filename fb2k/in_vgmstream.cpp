/* Winamp plugin interface for vgmstream */
/* Based on: */
/*
** Example Winamp .RAW input plug-in
** Copyright (c) 1998, Justin Frankel/Nullsoft Inc.
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>

#include <foobar2000.h>
#include <helpers.h>
#include <shared.h>

extern "C" {
#include "../src/vgmstream.h"
#include "../src/util.h"
}
#include "foo_vgmstream.h"

#ifndef VERSION
#define VERSION
#endif

#define APP_NAME "vgmstream plugin"
#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__
#define PLUGIN_VERSION VERSION " " __DATE__
#define INI_NAME "plugin.ini"

#define DECODE_SIZE		1024

/* format detection and VGMSTREAM setup, uses default parameters */
VGMSTREAM * init_vgmstream_foo(const char * const filename, abort_callback & p_abort) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *streamFile = open_foo_streamfile(filename, &p_abort);
    if (streamFile) {
        vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
        close_streamfile(streamFile);
    }
    return vgmstream;
}

class input_vgmstream {
	public:
		void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort) {

			currentreason = p_reason;
			if(p_path) strcpy(filename, p_path);

			switch(p_reason) {
				case input_open_decode:
				    vgmstream = init_vgmstream_foo(p_path, p_abort);

			    	/* were we able to open it? */
    				if (!vgmstream) {
        				return;
    				}

    				if (ignore_loop) vgmstream->loop_flag = 0;

    				/* will we be able to play it? */

				    if (vgmstream->channels <= 0) {
        				close_vgmstream(vgmstream);
        				vgmstream=NULL;
        				return;
    				}

    				decode_abort = 0;
    				seek_needed_samples = -1;
    				decode_pos_ms = 0;
    				decode_pos_samples = 0;
    				paused = 0;
    				stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);

    				fade_samples = (int)(fade_seconds * vgmstream->sample_rate);

					break;

				case input_open_info_read:
					break;

				case input_open_info_write:
					//cant write...ever
					break;

				default:
					break;
			}
		}

		void get_info(file_info & p_info,abort_callback & p_abort ) {
			int length_in_ms=0, channels = 0, samplerate = 0;
			char title[256];

			getfileinfo(filename, title, &length_in_ms, &samplerate, &channels, p_abort);

			p_info.info_set_int("samplerate", samplerate);
			p_info.info_set_int("channels", channels);
			p_info.info_set_int("bitspersample",16);
			p_info.info_set("encoding","lossless");
			p_info.info_set_bitrate(16);

			p_info.set_length(((double)length_in_ms)/1000);
		}

		void decode_initialize(unsigned p_flags,abort_callback & p_abort) { };

		bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
            int l = 0, samples_to_do = DECODE_SIZE, t= 0;

            if(vgmstream && (seek_needed_samples == -1 )) {

				if (decode_pos_samples+DECODE_SIZE>stream_length_samples && (!loop_forever || !vgmstream->loop_flag))
					samples_to_do=stream_length_samples-decode_pos_samples;
        		else
            		samples_to_do=DECODE_SIZE;

				l = (samples_to_do*vgmstream->channels*2);

            	if (samples_to_do /*< DECODE_SIZE*/ == 0) {
            		return false;
            	}

            	t = ((test_length*vgmstream->sample_rate)/1000);

            	render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            	/* fade! */
                if (vgmstream->loop_flag && fade_samples > 0 && !loop_forever) {
                    int samples_into_fade = decode_pos_samples - (stream_length_samples - fade_samples);
                    if (samples_into_fade + samples_to_do > 0) {
                        int j,k;
                        for (j=0;j<samples_to_do;j++,samples_into_fade++) {
                            if (samples_into_fade > 0) {
                                double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                                for (k=0;k<vgmstream->channels;k++) {
                                    sample_buffer[j*vgmstream->channels+k] =
                                        (short)(sample_buffer[j*vgmstream->channels+k]*fadedness);
                                }
                            }
                        }
                    }
                }

            	p_chunk.set_data_fixedpoint((char*)sample_buffer, l, vgmstream->sample_rate,vgmstream->channels,16,audio_chunk::g_guess_channel_config(vgmstream->channels));

            	decode_pos_samples+=samples_to_do;
            	decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;

            	return true;

            }

            return false;
		}

		void decode_seek(double p_seconds,abort_callback & p_abort) {
		}

		input_vgmstream() {
			vgmstream = NULL;
			decode_thread_handle = INVALID_HANDLE_VALUE;
			paused = 0;
			decode_abort = 0;
			seek_needed_samples = -1;
			decode_pos_ms = 0;
			decode_pos_samples = 0;
			stream_length_samples = 0;
			fade_samples = 0;

			fade_seconds = 10.0f;
			fade_delay_seconds = 0.0f;
			loop_count = 2.0f;
			thread_priority = 3;
			loop_forever = 0;
			ignore_loop = 0;

		}

		~input_vgmstream() {
		}

		t_filestats get_file_stats(abort_callback & p_abort) {
			t_filestats t;
			return t;
		}

		bool decode_can_seek() {return false;} /*not implemented yet*/
		bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {
			//do we need anything else 'cept the samplrate
			return true;
		}
		bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return false;}
		void decode_on_idle(abort_callback & p_abort) {/*m_file->on_idle(p_abort);*/}

		void retag(const file_info & p_info,abort_callback & p_abort) {};

		static bool g_is_our_content_type(const char * p_content_type) {return false;}
		static bool g_is_our_path(const char * p_path,const char * p_extension) {
			if(!stricmp_utf8(p_extension,"adx")) return 1;
            if(!stricmp_utf8(p_extension,"afc")) return 1;
            if(!stricmp_utf8(p_extension,"agsc")) return 1;
            if(!stricmp_utf8(p_extension,"ast")) return 1;
            if(!stricmp_utf8(p_extension,"brstm")) return 1;
            if(!stricmp_utf8(p_extension,"brstmspm")) return 1;
            if(!stricmp_utf8(p_extension,"hps")) return 1;
            if(!stricmp_utf8(p_extension,"strm")) return 1;
            if(!stricmp_utf8(p_extension,"adp")) return 1;
            if(!stricmp_utf8(p_extension,"rsf")) return 1;
            if(!stricmp_utf8(p_extension,"dsp")) return 1;
            if(!stricmp_utf8(p_extension,"gcw")) return 1;
            if(!stricmp_utf8(p_extension,"ads")) return 1;
            if(!stricmp_utf8(p_extension,"ss2")) return 1;
            if(!stricmp_utf8(p_extension,"npsf")) return 1;
            if(!stricmp_utf8(p_extension,"rwsd")) return 1;
            if(!stricmp_utf8(p_extension,"xa")) return 1;
            if(!stricmp_utf8(p_extension,"rxw")) return 1;
            if(!stricmp_utf8(p_extension,"int")) return 1;
            if(!stricmp_utf8(p_extension,"sts")) return 1;
            if(!stricmp_utf8(p_extension,"svag")) return 1;
            if(!stricmp_utf8(p_extension,"mib")) return 1;
            if(!stricmp_utf8(p_extension,"mi4")) return 1;
            if(!stricmp_utf8(p_extension,"mpdsp")) return 1;
            if(!stricmp_utf8(p_extension,"mic")) return 1;
            if(!stricmp_utf8(p_extension,"gcm")) return 1;
           	if(!stricmp_utf8(p_extension,"mss")) return 1;
			if(!stricmp_utf8(p_extension,"raw")) return 1;
			if(!stricmp_utf8(p_extension,"vag")) return 1;
			if(!stricmp_utf8(p_extension,"gms")) return 1;
			if(!stricmp_utf8(p_extension,"str")) return 1;
			if(!stricmp_utf8(p_extension,"ild")) return 1;
			if(!stricmp_utf8(p_extension,"pnb")) return 1;
			if(!stricmp_utf8(p_extension,"wavm")) return 1;
			if(!stricmp_utf8(p_extension,"xwav")) return 1;
			if(!stricmp_utf8(p_extension,"wp2")) return 1;
			if(!stricmp_utf8(p_extension,"pnb")) return 1;
			if(!stricmp_utf8(p_extension,"str")) return 1;
			if(!stricmp_utf8(p_extension,"sng")) return 1;
			if(!stricmp_utf8(p_extension,"asf")) return 1;
			if(!stricmp_utf8(p_extension,"eam")) return 1;
			if(!stricmp_utf8(p_extension,"cfn")) return 1;
			if(!stricmp_utf8(p_extension,"vpk")) return 1;
			if(!stricmp_utf8(p_extension,"genh")) return 1;
			return 0;
		}

	public:
		service_ptr_t<file> m_file;
		char filename[260];
		t_input_open_reason currentreason;
		VGMSTREAM * vgmstream;
		HANDLE decode_thread_handle;
		int paused;
		int decode_abort;
		int seek_needed_samples;
		int decode_pos_ms;
		int decode_pos_samples;
		int stream_length_samples;
		int fade_samples;
		int test_length;

		double fade_seconds;
		double fade_delay_seconds;
		double loop_count;
		int thread_priority;
		int loop_forever;
		int ignore_loop;

		short sample_buffer[576*2*2]; /* 576 16-bit samples, stereo, possibly doubled in size for DSP */

		void getfileinfo(char *filename, char *title, int *length_in_ms, int *sample_rate, int *channels, abort_callback & p_abort);
};


/* retrieve information on this or possibly another file */
void input_vgmstream::getfileinfo(char *filename, char *title, int *length_in_ms, int *sample_rate, int *channels, abort_callback & p_abort) {

    VGMSTREAM * infostream;
    if (length_in_ms)
    {
        *length_in_ms=-1000;
        if ((infostream=init_vgmstream_foo(filename, p_abort)))
        {
            *length_in_ms = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,infostream)*1000LL/infostream->sample_rate;
            test_length = *length_in_ms;
            *sample_rate = infostream->sample_rate;
            *channels = infostream->channels;

            close_vgmstream(infostream);
            infostream=NULL;
        }
    }
    if (title)
    {
        char *p=filename+strlen(filename);
        while (*p != '\\' && p >= filename) p--;
        strcpy(title,++p);
    }

}


static input_singletrack_factory_t<input_vgmstream> g_input_vgmstream_factory;

DECLARE_COMPONENT_VERSION(APP_NAME,PLUGIN_VERSION,PLUGIN_DESCRIPTION);

DECLARE_MULTIPLE_FILE_TYPE("ADX Audio File (*.ADX)", adx);
DECLARE_MULTIPLE_FILE_TYPE("AFC Audio File (*.AFC)", afc);
DECLARE_MULTIPLE_FILE_TYPE("AGSC Audio File (*.AGSC)", agsc);
DECLARE_MULTIPLE_FILE_TYPE("AST Audio File (*.AST)", ast);
DECLARE_MULTIPLE_FILE_TYPE("BRSTM Audio File (*.BRSTM)", brstm);
DECLARE_MULTIPLE_FILE_TYPE("BRSTM Audio File (*.BRSTMSPM)", brstmspm);
DECLARE_MULTIPLE_FILE_TYPE("HALPST Audio File (*.HPS)", hps);
DECLARE_MULTIPLE_FILE_TYPE("STRM Audio File (*.STRM)", strm);
DECLARE_MULTIPLE_FILE_TYPE("ADP Audio File (*.ADP)", adp);
DECLARE_MULTIPLE_FILE_TYPE("RSF Audio File (*.RSF)", rsf);
DECLARE_MULTIPLE_FILE_TYPE("DSP Audio File (*.DSP)", dsp);
DECLARE_MULTIPLE_FILE_TYPE("GCW Audio File (*.GCW)", gcw);
DECLARE_MULTIPLE_FILE_TYPE("PS2 ADS Audio File (*.ADS)", ads);
DECLARE_MULTIPLE_FILE_TYPE("PS2 SS2 Audio File (*.SS2)", ss2);
DECLARE_MULTIPLE_FILE_TYPE("PS2 NPSF Audio File (*.NPSF)", npsf);
DECLARE_MULTIPLE_FILE_TYPE("RWSD Audio File (*.RWSD)", rwsd);
DECLARE_MULTIPLE_FILE_TYPE("PSX CD-XA File (*.XA)", xa);
DECLARE_MULTIPLE_FILE_TYPE("PS2 RXWS File (*.RXW)", rxw);
DECLARE_MULTIPLE_FILE_TYPE("PS2 RAW Interleaved PCM (*.INT)", int);
DECLARE_MULTIPLE_FILE_TYPE("PS2 EXST Audio File (*.STS)", sts);
DECLARE_MULTIPLE_FILE_TYPE("PS2 SVAG Audio File (*.SVAG)", svag);
DECLARE_MULTIPLE_FILE_TYPE("PS2 MIB Audio File (*.MIB)", mib);
DECLARE_MULTIPLE_FILE_TYPE("PS2 MI4 Audio File (*.MI4)", mi4);
DECLARE_MULTIPLE_FILE_TYPE("MPDSP Audio File (*.MPDSP)", mpdsp);
DECLARE_MULTIPLE_FILE_TYPE("PS2 MIC Audio File (*.MIC)", mic);
DECLARE_MULTIPLE_FILE_TYPE("GCM Audio File (*.GCM)", gcm);
DECLARE_MULTIPLE_FILE_TYPE("MSS Audio File (*.MSS)", mss);
DECLARE_MULTIPLE_FILE_TYPE("RAW Audio File (*.RAW)", raw);
DECLARE_MULTIPLE_FILE_TYPE("VAG Audio File (*.VAG)", vag);
DECLARE_MULTIPLE_FILE_TYPE("GMS Audio File (*.GMS)", gms);
DECLARE_MULTIPLE_FILE_TYPE("STR Audio File (*.STR)", str);

