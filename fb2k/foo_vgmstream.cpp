/**
 * vgmstream for foobar2000
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
#include "../src/formats.h"
#include "../src/vgmstream.h"
}
#include "foo_vgmstream.h"
#include "foo_filetypes.h"

#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define PLUGIN_VERSION  __DATE__
#else
#define PLUGIN_VERSION  VERSION
#endif

#define APP_NAME "vgmstream plugin"
#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__ "\n" \
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm and many others\n" \
            "\n" \
            "foobar2000 plugin by Josh W, kode54\n" \
            "\n" \
            "https://github.com/kode54/vgmstream/\n" \
            "https://sourceforge.net/projects/vgmstream/ (original)";



/* format detection and VGMSTREAM setup, uses default parameters */
VGMSTREAM * input_vgmstream::init_vgmstream_foo(const char * const filename, abort_callback & p_abort) {
	VGMSTREAM *vgmstream = NULL;

	STREAMFILE *streamFile = open_foo_streamfile(filename, &p_abort, &stats);
	if (streamFile) {
		vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
		close_streamfile(streamFile);
	}
	return vgmstream;
}


void input_vgmstream::open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort) {

	currentreason = p_reason;
	if(p_path) filename = p_path;

	/* KLUDGE */
	if ( !pfc::stricmp_ascii( pfc::string_extension( p_path ), "MUS" ) )
	{
		unsigned char buffer[ 4 ];
		if ( p_filehint.is_empty() ) input_open_file_helper( p_filehint, p_path, p_reason, p_abort );
		p_filehint->read_object_t( buffer, p_abort );
		if ( !memcmp( buffer, "MUS\x1A", 4 ) ) throw exception_io_unsupported_format();
	}

	switch(p_reason) {
		case input_open_decode:
			vgmstream = init_vgmstream_foo(p_path, p_abort);

			/* were we able to open it? */
			if (!vgmstream) {
				/* Generate exception if the file is unopenable*/
				throw exception_io_data();
				return;
			}

			if (ignore_loop) vgmstream->loop_flag = 0;

			/* will we be able to play it? */

			if (vgmstream->channels <= 0) {
				close_vgmstream(vgmstream);
				vgmstream=NULL;
				throw exception_io_data();
				return;
			}

			decode_pos_ms = 0;
			decode_pos_samples = 0;
			paused = 0;
			stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);

			fade_samples = (int)(fade_seconds * vgmstream->sample_rate);

			break;

		case input_open_info_read:
			if ( p_filehint.is_empty() ) input_open_file_helper( p_filehint, p_path, p_reason, p_abort );
			stats = p_filehint->get_stats( p_abort );
			break;

		case input_open_info_write:
			//cant write...ever
			throw exception_io_data();
			break;

		default:
			break;
	}
}

void input_vgmstream::get_info(file_info & p_info,abort_callback & p_abort ) {
	int length_in_ms=0, channels = 0, samplerate = 0;
	int total_samples = -1;
	int bitrate = 0;
	int loop_start = -1, loop_end = -1;
	pfc::string8 description;
	pfc::string8_fast temp;

	getfileinfo(filename, NULL, &length_in_ms, &total_samples, &loop_start, &loop_end, &samplerate, &channels, &bitrate, description, p_abort);

	p_info.info_set_int("samplerate", samplerate);
	p_info.info_set_int("channels", channels);
	p_info.info_set_int("bitspersample",16);
	p_info.info_set("encoding","lossless");
	p_info.info_set_bitrate(bitrate / 1000);
	if (total_samples > 0)
		p_info.info_set_int("stream_total_samples", total_samples);
	if (loop_start >= 0 && loop_end >= loop_start)
	{
		p_info.info_set_int("loop_start", loop_start);
		p_info.info_set_int("loop_end", loop_end);
	}

	t_size pos = description.find_first("encoding: ");
	t_size eos;
	if (pos != pfc::infinite_size)
	{
		pos += strlen("encoding: ");
		eos = description.find_first('\n', pos);
		if (eos == pfc::infinite_size) eos = description.length();
		temp.set_string(description + pos, eos - pos);
		p_info.info_set("codec", temp);
	}

	pos = description.find_first("layout: ");
	if (pos != pfc::infinite_size)
	{
		pos += strlen("layout: ");
		eos = description.find_first('\n', pos);
		if (eos == pfc::infinite_size) eos = description.length();
		temp.set_string(description + pos, eos - pos);
		p_info.info_set("layout", temp);
	}

	pos = description.find_first("interleave: ");
	if (pos != pfc::infinite_size)
	{
		pos += strlen("interleave: ");
		eos = description.find_first(' ', pos);
		if (eos == pfc::infinite_size) eos = description.length();
		temp.set_string(description + pos, eos - pos);
		p_info.info_set("interleave", temp);
		pos = description.find_first("last block interleave: ", eos);
		if (pos != pfc::infinite_size)
		{
			pos += strlen("last block interleave: ");
			eos = description.find_first(' ', pos);
			if (eos == pfc::infinite_size) eos = description.length();
			temp.set_string(description + pos, eos - pos);
			p_info.info_set("interleave last block", temp);
		}
	}

	pos = description.find_first("metadata from: ");
	if (pos != pfc::infinite_size)
	{
		pos += strlen("metadata from: ");
		eos = description.find_first('\n', pos);
		if (eos == pfc::infinite_size) eos = description.length();
		temp.set_string(description + pos, eos - pos);
		p_info.info_set("metadata source", temp);
	}

    pos = description.find_first("number of streams: ");
    if (pos != pfc::infinite_size)
    {
        pos += strlen("number of streams: ");
        eos = description.find_first('\n', pos);
        if (eos == pfc::infinite_size) eos = description.length();
        temp.set_string(description + pos, eos - pos);
        p_info.info_set("number of streams", temp);
    }

    pos = description.find_first("block size: ");
    if (pos != pfc::infinite_size)
    {
        pos += strlen("block size: ");
        eos = description.find_first('\n', pos);
        if (eos == pfc::infinite_size) eos = description.length();
        temp.set_string(description + pos, eos - pos);
        p_info.info_set("block size", temp);
    }

	p_info.set_length(((double)length_in_ms)/1000);
}

void input_vgmstream::decode_initialize(unsigned p_flags,abort_callback & p_abort) {
	force_ignore_loop = !!(p_flags & input_flag_no_looping);
	decode_seek( 0, p_abort );
};

bool input_vgmstream::decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
	if (!decoding) return false;

	int max_buffer_samples = sizeof(sample_buffer)/sizeof(sample_buffer[0])/vgmstream->channels;
	int l = 0, samples_to_do = max_buffer_samples, t= 0;

	if(vgmstream) {
		bool loop_okay = loop_forever && vgmstream->loop_flag && !ignore_loop && !force_ignore_loop;
		if (decode_pos_samples+max_buffer_samples>stream_length_samples && !loop_okay)
			samples_to_do=stream_length_samples-decode_pos_samples;
		else
			samples_to_do=max_buffer_samples;

		l = (samples_to_do*vgmstream->channels * sizeof(sample_buffer[0]));

		if (samples_to_do /*< DECODE_SIZE*/ == 0) {
			decoding = false;
			return false;
		}

		t = ((test_length*vgmstream->sample_rate)/1000);

		render_vgmstream(sample_buffer,samples_to_do,vgmstream);

		/* fade! */
		if (vgmstream->loop_flag && fade_samples > 0 && !loop_okay) {
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

		p_chunk.set_data_fixedpoint((char*)sample_buffer, l, vgmstream->sample_rate, vgmstream->channels, 16, audio_chunk::g_guess_channel_config(vgmstream->channels));

		decode_pos_samples+=samples_to_do;
		decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;

		return samples_to_do==max_buffer_samples;

	}
	return false;
}

void input_vgmstream::decode_seek(double p_seconds,abort_callback & p_abort) {
	seek_pos_samples = (int) audio_math::time_to_samples(p_seconds, vgmstream->sample_rate);
	int max_buffer_samples = sizeof(sample_buffer)/sizeof(sample_buffer[0])/vgmstream->channels;
	bool loop_okay = loop_forever && vgmstream->loop_flag && !ignore_loop && !force_ignore_loop;

	int corrected_pos_samples = seek_pos_samples;

	// adjust for correct position within loop
	if(vgmstream->loop_flag && (vgmstream->loop_end_sample - vgmstream->loop_start_sample) && seek_pos_samples >= vgmstream->loop_end_sample) {
		corrected_pos_samples -= vgmstream->loop_start_sample;
		corrected_pos_samples %= (vgmstream->loop_end_sample - vgmstream->loop_start_sample);
		corrected_pos_samples += vgmstream->loop_start_sample;
	}

	// Allow for delta seeks forward, by up to the total length of the stream, if the delta is less than the corrected offset
	if(decode_pos_samples > corrected_pos_samples && decode_pos_samples <= seek_pos_samples &&
	   (seek_pos_samples - decode_pos_samples) < stream_length_samples) {
		if (corrected_pos_samples > (seek_pos_samples - decode_pos_samples))
			corrected_pos_samples = seek_pos_samples;
	}

	// Reset of backwards seek
	else if(corrected_pos_samples < decode_pos_samples) {
		reset_vgmstream(vgmstream);
		if (ignore_loop) vgmstream->loop_flag = 0;
		decode_pos_samples = 0;
	}
	
	// seeking overrun = bad
	if(corrected_pos_samples > stream_length_samples) corrected_pos_samples = stream_length_samples;

	while(decode_pos_samples<corrected_pos_samples) {
		int seek_samples = max_buffer_samples;
		if((decode_pos_samples+max_buffer_samples>=stream_length_samples) && !loop_okay)
			seek_samples=stream_length_samples-seek_pos_samples;
		if(decode_pos_samples+max_buffer_samples>seek_pos_samples)
			seek_samples=seek_pos_samples-decode_pos_samples;

		decode_pos_samples+=seek_samples;
		render_vgmstream(sample_buffer,seek_samples,vgmstream);
	}

	// remove seek loop correction from counter so file ends correctly
	decode_pos_samples=seek_pos_samples;

	decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;

	decoding = loop_okay || decode_pos_samples < stream_length_samples;
}


input_vgmstream::input_vgmstream() {
	vgmstream = NULL;
	paused = 0;
	decode_pos_ms = 0;
	decode_pos_samples = 0;
	stream_length_samples = 0;
	fade_samples = 0;
	fade_seconds = 10.0f;
	fade_delay_seconds = 0.0f;
	loop_count = 2.0f;
	loop_forever = false;
	ignore_loop = 0;
	decoding = false;
	seek_pos_samples = 0;
	load_settings();

}

input_vgmstream::~input_vgmstream() {
	if(vgmstream)
		close_vgmstream(vgmstream);
	vgmstream=NULL;
}

t_filestats input_vgmstream::get_file_stats(abort_callback & p_abort) {
	return stats;
}

bool input_vgmstream::decode_can_seek() {return true;}
bool input_vgmstream::decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {
	//do we need anything else 'cept the samplrate
	return true;
}
bool input_vgmstream::decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return false;}
void input_vgmstream::decode_on_idle(abort_callback & p_abort) {/*m_file->on_idle(p_abort);*/}

void input_vgmstream::retag(const file_info & p_info,abort_callback & p_abort) {};

bool input_vgmstream::g_is_our_content_type(const char * p_content_type) {return false;}
bool input_vgmstream::g_is_our_path(const char * p_path,const char * p_extension) {
    const char ** ext_list;
    int ext_list_len;
    int i;

    ext_list = vgmstream_get_formats();
    ext_list_len = vgmstream_get_formats_length();

    for (i=0; i < ext_list_len; i++) {
        if (!stricmp_utf8(p_extension, ext_list[i]))
            return 1;
    }

    return 0;
}




/* retrieve information on this or possibly another file */
void input_vgmstream::getfileinfo(const char *filename, char *title, int *length_in_ms, int *total_samples, int *loop_start, int *loop_end, int *sample_rate, int *channels, int *bitrate, pfc::string_base & description, abort_callback & p_abort) {
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
			*total_samples = infostream->num_samples;
			*bitrate = get_vgmstream_average_bitrate(infostream);
			if (infostream->loop_flag)
			{
				*loop_start = infostream->loop_start_sample;
				*loop_end = infostream->loop_end_sample;
			}

			char temp[1024];
			describe_vgmstream(infostream, temp, 1024);
			description = temp;

			close_vgmstream(infostream);
			infostream=NULL;
		}
	}
	if (title)
	{
		const char *p=filename+strlen(filename);
		while (*p != '\\' && p >= filename) p--;
		strcpy(title,++p);
	}

}


static input_singletrack_factory_t<input_vgmstream> g_input_vgmstream_factory;

DECLARE_COMPONENT_VERSION(APP_NAME,PLUGIN_VERSION,PLUGIN_DESCRIPTION);
VALIDATE_COMPONENT_FILENAME("foo_input_vgmstream.dll");
