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

// Registered file types, to associate an extension with foobar2000 in Windows.
// Accepted types go in input_vgmstream::g_is_our_path; both lists don't need to match.
// todo do we really want to associate every single vgmstream format?
//
// these are declared statically, and if anyone has a better idea i'd like to hear it - josh.
DECLARE_MULTIPLE_FILE_TYPE("2DX9 Audio File (*.2DX9)", 2dx9);
DECLARE_MULTIPLE_FILE_TYPE("2PFS Audio File (*.2PFS)", 2pfs);

DECLARE_MULTIPLE_FILE_TYPE("AAAP Audio File (*.AAAP)", aaap);
DECLARE_MULTIPLE_FILE_TYPE("AAX Audio File (*.AAX)", aax);

DECLARE_MULTIPLE_FILE_TYPE("ACM Audio File (*.ACM)", acm);
DECLARE_MULTIPLE_FILE_TYPE("ADPCM Audio File (*.ADPCM)", adpcm);
DECLARE_MULTIPLE_FILE_TYPE("ADM Audio File (*.ADM)", adm);
DECLARE_MULTIPLE_FILE_TYPE("ADP Audio File (*.ADP)", adp);
DECLARE_MULTIPLE_FILE_TYPE("PS2 ADS Audio File (*.ADS)", ads);
DECLARE_MULTIPLE_FILE_TYPE("ADX Audio File (*.ADX)", adx);
DECLARE_MULTIPLE_FILE_TYPE("AFC Audio File (*.AFC)", afc);
DECLARE_MULTIPLE_FILE_TYPE("AGSC Audio File (*.AGSC)", agsc);
DECLARE_MULTIPLE_FILE_TYPE("AHX Audio File (*.AHX)", ahx);
DECLARE_MULTIPLE_FILE_TYPE("AIFC Audio File (*.AIFC)", aifc);
DECLARE_MULTIPLE_FILE_TYPE("AIX Audio File (*.AIX)", aix);
DECLARE_MULTIPLE_FILE_TYPE("AKB Audio File (*.AKB)", akb);
DECLARE_MULTIPLE_FILE_TYPE("AMTS Audio File (*.AMTS)", amts);
DECLARE_MULTIPLE_FILE_TYPE("AS4 Audio File (*.AS4)", as4);
DECLARE_MULTIPLE_FILE_TYPE("ASD Audio File (*.ASD)", asd);
DECLARE_MULTIPLE_FILE_TYPE("ASF Audio File (*.ASF)", asf);
DECLARE_MULTIPLE_FILE_TYPE("AST Audio File (*.AST)", ast);
DECLARE_MULTIPLE_FILE_TYPE("ASR Audio File (*.ASR)", asr);
DECLARE_MULTIPLE_FILE_TYPE("ASS Audio File (*.ASS)", ass);
DECLARE_MULTIPLE_FILE_TYPE("ATRAC3plus Audio File (*.AT3)", at3);
DECLARE_MULTIPLE_FILE_TYPE("AUD Audio File (*.AUD)", aud);
DECLARE_MULTIPLE_FILE_TYPE("AUS Audio File (*.AUS)", aus);

DECLARE_MULTIPLE_FILE_TYPE("BAKA Audio File (*.BAKA)", baka);
DECLARE_MULTIPLE_FILE_TYPE("BAF Audio File (*.BAF)", baf);
DECLARE_MULTIPLE_FILE_TYPE("BAR Audio File (*.BAR)", bar);
DECLARE_MULTIPLE_FILE_TYPE("BCSTM Audio File (*.BCSTM)", bcstm);
DECLARE_MULTIPLE_FILE_TYPE("BCWAV Audio File (*.BCWAV)", bcwav);
DECLARE_MULTIPLE_FILE_TYPE("BFSTM Audio File (*.BFSTM)", bfstm);
DECLARE_MULTIPLE_FILE_TYPE("BFWAV Audio File (*.BFWAV)", bfwav);
DECLARE_MULTIPLE_FILE_TYPE("BFWAV Audio File [2] (*.BFWAV)", bfwavnsmbu);
DECLARE_MULTIPLE_FILE_TYPE("BG00 Audio File (*.BG00)", bg00);
DECLARE_MULTIPLE_FILE_TYPE("BGW Audio File (*.BGW)", bgw);
DECLARE_MULTIPLE_FILE_TYPE("BH2PCM Audio File (*.BH2PCM)", bh2pcm);
DECLARE_MULTIPLE_FILE_TYPE("BIK Audio File (*.BIK)", bik);
DECLARE_MULTIPLE_FILE_TYPE("BIKA Audio File (*.BIKA)", bika);
DECLARE_MULTIPLE_FILE_TYPE("BIK2 Audio File (*.BIK2)", bik2);
DECLARE_MULTIPLE_FILE_TYPE("BIK2A Audio File (*.BIK2A)", bik2a);
DECLARE_MULTIPLE_FILE_TYPE("BK2 Audio File (*.BK2)", bk2);
DECLARE_MULTIPLE_FILE_TYPE("BK2A Audio File (*.BK2A)", bk2a);
DECLARE_MULTIPLE_FILE_TYPE("BMDX Audio File (*.BMDX)", bmdx);
DECLARE_MULTIPLE_FILE_TYPE("BMS Audio File (*.BMS)", bms);
DECLARE_MULTIPLE_FILE_TYPE("KLBS Audio File (*.BNK)", bnk);
DECLARE_MULTIPLE_FILE_TYPE("BNS Audio File (*.BNS)", bns);
DECLARE_MULTIPLE_FILE_TYPE("BNSF Audio File (*.BNSF)", bnsf);
DECLARE_MULTIPLE_FILE_TYPE("BO2 Audio File (*.BO2)", bo2);
DECLARE_MULTIPLE_FILE_TYPE("BRSTM Audio File (*.BRSTM)", brstm);
DECLARE_MULTIPLE_FILE_TYPE("BRSTM Audio File [2] (*.BRSTM)", brstmspm);
DECLARE_MULTIPLE_FILE_TYPE("BTSND Audio File (*.BTSND)", btsnd);
DECLARE_MULTIPLE_FILE_TYPE("BVG Audio File (*.BVG)", bvg);

DECLARE_MULTIPLE_FILE_TYPE("CAF Audio File (*.CAF)", caf);
DECLARE_MULTIPLE_FILE_TYPE("CAPDSP Audio File (*.CAPDSP)", capdsp);
DECLARE_MULTIPLE_FILE_TYPE("CBD2 Audio File (*.CBD2)", cbd2);
DECLARE_MULTIPLE_FILE_TYPE("CCC Audio File (*.CCC)", ccc);
DECLARE_MULTIPLE_FILE_TYPE("CFN Audio File (*.CFN)", cfn);
DECLARE_MULTIPLE_FILE_TYPE("CKD Audio File (*.CKD)", ckd);
DECLARE_MULTIPLE_FILE_TYPE("CNK Audio File (*.CNK)", cnk);
DECLARE_MULTIPLE_FILE_TYPE("CPS Audio File (*.CPS)", cps);

DECLARE_MULTIPLE_FILE_TYPE("DCS Audio File (*.DCS)", dcs);
DECLARE_MULTIPLE_FILE_TYPE("DE2 Audio File (*.DE2)", de2);
DECLARE_MULTIPLE_FILE_TYPE("DDSP Audio File (*.DDSP)", ddsp);
DECLARE_MULTIPLE_FILE_TYPE("DMSG Audio File (*.DMSG)", dmsg);
DECLARE_MULTIPLE_FILE_TYPE("DSP Audio File (*.DSP)", dsp);
DECLARE_MULTIPLE_FILE_TYPE("DSPW Audio File (*.DSPW)", dspw);
DECLARE_MULTIPLE_FILE_TYPE("DTK Audio File (*.DTK)", dtk);
DECLARE_MULTIPLE_FILE_TYPE("DVI Audio File (*.DVI)", dvi);
DECLARE_MULTIPLE_FILE_TYPE("DXH Audio File (*.DXH)", dxh);

DECLARE_MULTIPLE_FILE_TYPE("EAM Audio File (*.EAM)", eam);
DECLARE_MULTIPLE_FILE_TYPE("EMFF Audio File (*.EMFF)", emff);
DECLARE_MULTIPLE_FILE_TYPE("ENTH Audio File (*.ENTH)", enth);

DECLARE_MULTIPLE_FILE_TYPE("FAG Audio File (*.FAG)", fag);
DECLARE_MULTIPLE_FILE_TYPE("FILP Audio File (*.FILP)", filp);
DECLARE_MULTIPLE_FILE_TYPE("FSB Audio File (*.FSB)", fsb);

DECLARE_MULTIPLE_FILE_TYPE("G1L Audio File (*.G1L)", g1l);
DECLARE_MULTIPLE_FILE_TYPE("GBTS Audio File (*.GBTS)", gbts);
DECLARE_MULTIPLE_FILE_TYPE("GCA Audio File (*.GCA)", gca);
DECLARE_MULTIPLE_FILE_TYPE("GCM Audio File (*.GCM)", gcm);
DECLARE_MULTIPLE_FILE_TYPE("GCUB Audio File (*.GCUB)", gcub);
DECLARE_MULTIPLE_FILE_TYPE("GCW Audio File (*.GCW)", gcw);
DECLARE_MULTIPLE_FILE_TYPE("GENH Audio File (*.GENH)", genh);
DECLARE_MULTIPLE_FILE_TYPE("GMS Audio File (*.GMS)", gms);
DECLARE_MULTIPLE_FILE_TYPE("GSB Audio File (*.GSB)", gsb);

DECLARE_MULTIPLE_FILE_TYPE("HCA Audio File (*.HCA)", hca);
DECLARE_MULTIPLE_FILE_TYPE("HGC1 Audio File (*.HGC1)", hgc1);
DECLARE_MULTIPLE_FILE_TYPE("HIS Audio File (*.HIS)", his);
DECLARE_MULTIPLE_FILE_TYPE("HLWAV Audio File (*.HLWAV)", hlwav);
DECLARE_MULTIPLE_FILE_TYPE("HALPST Audio File (*.HPS)", hps);
DECLARE_MULTIPLE_FILE_TYPE("HSF Audio File (*.HSF)", hsf);
DECLARE_MULTIPLE_FILE_TYPE("HWAS Audio File (*.HWAS)", hwas);

DECLARE_MULTIPLE_FILE_TYPE("IDSP Audio File (*.IDSP)", idsp);
DECLARE_MULTIPLE_FILE_TYPE("IDVI Audio File (*.IDVI)", idvi);
DECLARE_MULTIPLE_FILE_TYPE("IKM Audio File (*.IKM)", ikm);
DECLARE_MULTIPLE_FILE_TYPE("ILD Audio File (*.ILD)", ild);
DECLARE_MULTIPLE_FILE_TYPE("PS2 RAW Interleaved PCM (*.INT)", int);
DECLARE_MULTIPLE_FILE_TYPE("ISD Audio File (*.ISD)", isd);
DECLARE_MULTIPLE_FILE_TYPE("ISWS Audio File (*.ISWS)", isws);
DECLARE_MULTIPLE_FILE_TYPE("IVAUD Audio File (*.IVAUD)", ivaud);
DECLARE_MULTIPLE_FILE_TYPE("IVAG Audio File (*.IVAG)", ivag);
DECLARE_MULTIPLE_FILE_TYPE("IVB Audio File (*.IVB)", ivb);

DECLARE_MULTIPLE_FILE_TYPE("JOE Audio File (*.JOE)", joe);
DECLARE_MULTIPLE_FILE_TYPE("JSTM Audio File (*.JSTM)", jstm);

DECLARE_MULTIPLE_FILE_TYPE("KCES Audio File (*.KCES)", kces);
DECLARE_MULTIPLE_FILE_TYPE("KCEY Audio File (*.KCEY)", kcey);
DECLARE_MULTIPLE_FILE_TYPE("KHV Audio File (*.KHV)", khv);
DECLARE_MULTIPLE_FILE_TYPE("KOVS Audio File (*.KOVS)", kovs);
DECLARE_MULTIPLE_FILE_TYPE("KRAW Audio File (*.KRAW)", kraw);

DECLARE_MULTIPLE_FILE_TYPE("LEG Audio File (*.LEG)", leg);
DECLARE_MULTIPLE_FILE_TYPE("LOGG Audio File (*.LOGG)", logg);
DECLARE_MULTIPLE_FILE_TYPE("LMP4 Audio File (*.LMP4)", lmp4);
DECLARE_MULTIPLE_FILE_TYPE("LPCM Audio File (*.LPCM)", lpcm);
DECLARE_MULTIPLE_FILE_TYPE("LPS Audio File (*.LPS)", lps);
DECLARE_MULTIPLE_FILE_TYPE("LSF Audio File (*.LSF)", lsf);
DECLARE_MULTIPLE_FILE_TYPE("LWAV Audio File (*.LWAV)", lwav);

DECLARE_MULTIPLE_FILE_TYPE("MATX Audio File (*.MATX)", matx);
DECLARE_MULTIPLE_FILE_TYPE("MC3 Audio File (*.MC3)", mc3);
DECLARE_MULTIPLE_FILE_TYPE("MCA Audio File (*.MCA)", mca);
DECLARE_MULTIPLE_FILE_TYPE("MCG Audio File (*.MCG)", mcg);
DECLARE_MULTIPLE_FILE_TYPE("MDS Audio File (*.MDS)", mds);
DECLARE_MULTIPLE_FILE_TYPE("PS2 MI4 Audio File (*.MI4)", mi4);
DECLARE_MULTIPLE_FILE_TYPE("PS2 MIB Audio File (*.MIB)", mib);
DECLARE_MULTIPLE_FILE_TYPE("PS2 MIC Audio File (*.MIC)", mic);
DECLARE_MULTIPLE_FILE_TYPE("MIHB Audio File (*.MIHB)", mihb);
DECLARE_MULTIPLE_FILE_TYPE("MNSTR Audio File (*.MNSTR)", mnstr);
DECLARE_MULTIPLE_FILE_TYPE("MPDSP Audio File (*.MPDSP)", mpdsp);
DECLARE_MULTIPLE_FILE_TYPE("MPDS Audio File (*.MPDS)", mpds);
DECLARE_MULTIPLE_FILE_TYPE("MSA Audio File (*.MSA)", msa);
DECLARE_MULTIPLE_FILE_TYPE("MSF Audio File (*.MSF)", msf);
DECLARE_MULTIPLE_FILE_TYPE("MSS Audio File (*.MSS)", mss);
DECLARE_MULTIPLE_FILE_TYPE("MSVP Audio File (*.MSVP)", msvp);
DECLARE_MULTIPLE_FILE_TYPE("MTAF Audio File (*.MTAF)", mtaf);
DECLARE_MULTIPLE_FILE_TYPE("MUS Playlist File (*.MUS)", mus);
DECLARE_MULTIPLE_FILE_TYPE("MUSC Audio File (*.MUSC)", musc);
DECLARE_MULTIPLE_FILE_TYPE("MUSX Audio File (*.MUSX)", musx);
DECLARE_MULTIPLE_FILE_TYPE("MWV Audio File (*.MWV)", mwv);
DECLARE_MULTIPLE_FILE_TYPE("MxSt Audio File (*.MxSt)", mxst);
DECLARE_MULTIPLE_FILE_TYPE("MYSPD Audio File (*.MYSPD)", myspd);

DECLARE_MULTIPLE_FILE_TYPE("NDP Audio File (*.NDP)", ndp);
DECLARE_MULTIPLE_FILE_TYPE("NGCA Audio File (*.NGCA)", ngca);
DECLARE_MULTIPLE_FILE_TYPE("PS2 NPSF Audio File (*.NPSF)", npsf);
DECLARE_MULTIPLE_FILE_TYPE("NUS3BANK Audio File (*.NUS3BANK)", nus3bank);
DECLARE_MULTIPLE_FILE_TYPE("NWA Audio File (*.NWA)", nwa);

DECLARE_MULTIPLE_FILE_TYPE("OGL Audio File (*.OGL)", ogl);
DECLARE_MULTIPLE_FILE_TYPE("OMU Audio File (*.OMU)", omu);
DECLARE_MULTIPLE_FILE_TYPE("OTM Audio File (*.OTM)", otm);

DECLARE_MULTIPLE_FILE_TYPE("P2BT Audio File (*.P2BT)", p2bt);
DECLARE_MULTIPLE_FILE_TYPE("P3D Audio File (*.P3D)", p3d);
DECLARE_MULTIPLE_FILE_TYPE("PAST Audio File (*.PAST)", past);
DECLARE_MULTIPLE_FILE_TYPE("PCM Audio File (*.PCM)", pcm);
DECLARE_MULTIPLE_FILE_TYPE("PDT Audio File (*.PDT)", pdt);
DECLARE_MULTIPLE_FILE_TYPE("PNB Audio File (*.PNB)", pnb);
DECLARE_MULTIPLE_FILE_TYPE("PONA Audio File (*.PONA)", pona);
DECLARE_MULTIPLE_FILE_TYPE("POS Audio File (*.POS)", pos);
DECLARE_MULTIPLE_FILE_TYPE("PS2STM Audio File (*.PS2STM)", ps2stm);
DECLARE_MULTIPLE_FILE_TYPE("PSH Audio File (*.PSH)", psh);
DECLARE_MULTIPLE_FILE_TYPE("PSND Audio File (*.PSND)", psnd);
DECLARE_MULTIPLE_FILE_TYPE("PSW Audio File (*.PSW)", psw);

DECLARE_MULTIPLE_FILE_TYPE("RAK Audio File (*.RAK)", rak);
DECLARE_MULTIPLE_FILE_TYPE("RAS Audio File (*.RAS)", ras);
DECLARE_MULTIPLE_FILE_TYPE("RAW Audio File (*.RAW)", raw);
DECLARE_MULTIPLE_FILE_TYPE("RKV Audio File (*.RKV)", rkv);
DECLARE_MULTIPLE_FILE_TYPE("RND Audio File (*.RND)", rnd);
DECLARE_MULTIPLE_FILE_TYPE("RRDS Audio File (*.RRDS)", rrds);
DECLARE_MULTIPLE_FILE_TYPE("RSD Audio File (*.RSD)", rsd);
DECLARE_MULTIPLE_FILE_TYPE("RSF Audio File (*.RSF)", rsf);
DECLARE_MULTIPLE_FILE_TYPE("RSTM Audio File (*.RSTM)", rstm);
DECLARE_MULTIPLE_FILE_TYPE("RVWS Audio File (*.RSTM)", rvws);
DECLARE_MULTIPLE_FILE_TYPE("RWAR Audio File (*.RWSD)", rwar);
DECLARE_MULTIPLE_FILE_TYPE("RWAV Audio File (*.RWAV)", rwav);
DECLARE_MULTIPLE_FILE_TYPE("RWS Audio File (*.RWS)", rws);
DECLARE_MULTIPLE_FILE_TYPE("RWSD Audio File (*.RWSD)", rwsd);
DECLARE_MULTIPLE_FILE_TYPE("RWX Audio File (*.RWX)", rwx);
DECLARE_MULTIPLE_FILE_TYPE("PS2 RXWS File (*.RXW)", rxw);

DECLARE_MULTIPLE_FILE_TYPE("S14 Audio File (*.S14)", s14);
DECLARE_MULTIPLE_FILE_TYPE("SAB Audio File (*.SAB)", sab);
DECLARE_MULTIPLE_FILE_TYPE("SAD Audio File (*.SAD)", sad);
DECLARE_MULTIPLE_FILE_TYPE("SAP Audio File (*.SAP)", sap);
DECLARE_MULTIPLE_FILE_TYPE("SC Audio File (*.SC)", sc);
DECLARE_MULTIPLE_FILE_TYPE("SCD Audio File (*.SCD)", scd);
DECLARE_MULTIPLE_FILE_TYPE("SCK Audio File (*.SCK)", sck);
DECLARE_MULTIPLE_FILE_TYPE("SD9 Audio File (*.SD9)", sd9);
DECLARE_MULTIPLE_FILE_TYPE("SDT Audio File (*.SDT)", sdt);
DECLARE_MULTIPLE_FILE_TYPE("SEG Audio File (*.SEG)", seg);
DECLARE_MULTIPLE_FILE_TYPE("SF0 Audio File (*.SF0)", sf0);
DECLARE_MULTIPLE_FILE_TYPE("SFL Audio File (*.SFL)", sfl);
DECLARE_MULTIPLE_FILE_TYPE("SFS Audio File (*.SFS)", sfs);
DECLARE_MULTIPLE_FILE_TYPE("SFX Audio File (*.SFX)", sfx);
DECLARE_MULTIPLE_FILE_TYPE("SGB Audio File (*.SGB)", sgb);
DECLARE_MULTIPLE_FILE_TYPE("SGD Audio File (*.SGD)", sgd);
DECLARE_MULTIPLE_FILE_TYPE("SGX Audio File (*.SGX)", sgx);
DECLARE_MULTIPLE_FILE_TYPE("SL3 Audio File (*.SL3)", sl3);
DECLARE_MULTIPLE_FILE_TYPE("SLI Audio File (*.SLI)", sli);
DECLARE_MULTIPLE_FILE_TYPE("SMP Audio File (*.SMP)", smp);
DECLARE_MULTIPLE_FILE_TYPE("SMPL Audio File (*.SMPL)", smpl);
DECLARE_MULTIPLE_FILE_TYPE("SND Audio File (*.SND)", snd);
DECLARE_MULTIPLE_FILE_TYPE("SNDS Audio File (*.SNDS)", snds);
DECLARE_MULTIPLE_FILE_TYPE("SNG Audio File (*.SNG)", sng);
DECLARE_MULTIPLE_FILE_TYPE("SNS Audio File (*.SNS)", sns);
DECLARE_MULTIPLE_FILE_TYPE("SPD Audio File (*.SPD)", spd);
DECLARE_MULTIPLE_FILE_TYPE("SPM Audio File (*.SPM)", spm);
DECLARE_MULTIPLE_FILE_TYPE("SPS Audio File (*.SPS)", sps);
DECLARE_MULTIPLE_FILE_TYPE("SPSD Audio File (*.SPSD)", spsd);
DECLARE_MULTIPLE_FILE_TYPE("SPW Audio File (*.SPW)", spw);
DECLARE_MULTIPLE_FILE_TYPE("PS2 SS2 Audio File (*.SS2)", ss2);
DECLARE_MULTIPLE_FILE_TYPE("SS3 Audio File (*.SS3)", ss3);
DECLARE_MULTIPLE_FILE_TYPE("SS7 Audio File (*.SS7)", ss7);
DECLARE_MULTIPLE_FILE_TYPE("SSM Audio File (*.SSM)", ssm);
DECLARE_MULTIPLE_FILE_TYPE("SSS Audio File (*.SSS)", sss);
DECLARE_MULTIPLE_FILE_TYPE("STER Audio File (*.STER)", ster);
DECLARE_MULTIPLE_FILE_TYPE("STMA Audio File (*.STMA)", stma);
DECLARE_MULTIPLE_FILE_TYPE("STR Audio File (*.STR)", str);
DECLARE_MULTIPLE_FILE_TYPE("STRM Audio File (*.STRM)", strm);
DECLARE_MULTIPLE_FILE_TYPE("PS2 EXST Audio File (*.STS)", sts);
DECLARE_MULTIPLE_FILE_TYPE("STX Audio File (*.STX)", stx);
DECLARE_MULTIPLE_FILE_TYPE("PS2 SVAG Audio File (*.SVAG)", svag);
DECLARE_MULTIPLE_FILE_TYPE("SVS Audio File (*.SVS)", svs);
DECLARE_MULTIPLE_FILE_TYPE("SWAV Audio File (*.SWAV)", swav);
DECLARE_MULTIPLE_FILE_TYPE("SWD Audio File (*.SWD)", swd);
DECLARE_MULTIPLE_FILE_TYPE("SXD Audio File (*.SXD)", sxd);
DECLARE_MULTIPLE_FILE_TYPE("SXD2 Audio File (*.SXD2)", sxd2);

DECLARE_MULTIPLE_FILE_TYPE("TEC Audio File (*.TEC)", tec);
DECLARE_MULTIPLE_FILE_TYPE("THP Audio File (*.THP)", thp);
DECLARE_MULTIPLE_FILE_TYPE("TK1 Audio File (*.TK1)", tk1);
DECLARE_MULTIPLE_FILE_TYPE("TK5 Audio File (*.TK5)", tk5);
DECLARE_MULTIPLE_FILE_TYPE("TUN Audio File (*.TUN)", tun);
DECLARE_MULTIPLE_FILE_TYPE("TYDSP Audio File (*.TYDSP)", tydsp);

DECLARE_MULTIPLE_FILE_TYPE("UM3 Audio File (*.UM3)", um3);

DECLARE_MULTIPLE_FILE_TYPE("VAG Audio File (*.VAG)", vag);
DECLARE_MULTIPLE_FILE_TYPE("VAS Audio File (*.VAS)", vas);
DECLARE_MULTIPLE_FILE_TYPE("VB Audio File (*.VB)", vb);
DECLARE_MULTIPLE_FILE_TYPE("VBK Audio File (*.VBK)", vbk);
DECLARE_MULTIPLE_FILE_TYPE("VGS Audio File (*.VGS)", vgs);
DECLARE_MULTIPLE_FILE_TYPE("VGV Audio File (*.VGV)", vgv);
DECLARE_MULTIPLE_FILE_TYPE("VIG Audio File (*.VIG)", vig);
DECLARE_MULTIPLE_FILE_TYPE("VMS Audio File (*.VMS)", vms);
DECLARE_MULTIPLE_FILE_TYPE("VOI Audio File (*.VOI)", voi);
DECLARE_MULTIPLE_FILE_TYPE("VPK Audio File (*.VPK)", vpk);
DECLARE_MULTIPLE_FILE_TYPE("VS Audio File (*.VS)", vs);
DECLARE_MULTIPLE_FILE_TYPE("VSF Audio File (*.VSF)", vsf);

DECLARE_MULTIPLE_FILE_TYPE("WAA Audio File (*.WAA)", waa);
DECLARE_MULTIPLE_FILE_TYPE("WAC Audio File (*.WAC)", wac);
DECLARE_MULTIPLE_FILE_TYPE("WAD Audio File (*.WAD)", wad);
DECLARE_MULTIPLE_FILE_TYPE("WAM Audio File (*.WAM)", wam);
DECLARE_MULTIPLE_FILE_TYPE("WAVM Audio File (*.WAVM)", wavm);
DECLARE_MULTIPLE_FILE_TYPE("WAS Audio File (*.WAS)", was);
DECLARE_MULTIPLE_FILE_TYPE("WEM Audio File (*.WEM)", wem);
DECLARE_MULTIPLE_FILE_TYPE("WII Audio File (*.WII)", wii);
DECLARE_MULTIPLE_FILE_TYPE("WMUS Audio File (*.WMUS)", wmus);
DECLARE_MULTIPLE_FILE_TYPE("WP2 Audio File (*.WP2)", wp2);
DECLARE_MULTIPLE_FILE_TYPE("WPD Audio File (*.WPD)", wpd);
DECLARE_MULTIPLE_FILE_TYPE("WSD Audio File (*.WSD)", wsd);
DECLARE_MULTIPLE_FILE_TYPE("WSI Audio File (*.WSI)", wsi);
DECLARE_MULTIPLE_FILE_TYPE("WVS Audio File (*.WVS)", wvs);

DECLARE_MULTIPLE_FILE_TYPE("PSX CD-XA File (*.XA)", xa);
DECLARE_MULTIPLE_FILE_TYPE("XA2 Audio File (*.XA2)", xa2);
DECLARE_MULTIPLE_FILE_TYPE("XA30 Audio File (*.XA30)", xa30);
DECLARE_MULTIPLE_FILE_TYPE("XAU Audio File (*.XAU)", xau);
DECLARE_MULTIPLE_FILE_TYPE("XMA Audio File (*.XMA)", xma);
DECLARE_MULTIPLE_FILE_TYPE("XMA2 Audio File (*.XMA2)", xma2);
DECLARE_MULTIPLE_FILE_TYPE("XMU Audio File (*.XMU)", xmu);
DECLARE_MULTIPLE_FILE_TYPE("XNB Audio File (*.XNB)", xnb);
DECLARE_MULTIPLE_FILE_TYPE("XSF Audio File (*.XSF)", xsf);
DECLARE_MULTIPLE_FILE_TYPE("XSS Audio File (*.XSS)", xss);
DECLARE_MULTIPLE_FILE_TYPE("XVAG Audio File (*.XVAG)", xvag);
DECLARE_MULTIPLE_FILE_TYPE("XVAS Audio File (*.XVAS)", xvas);
DECLARE_MULTIPLE_FILE_TYPE("XWAV Audio File (*.XWAV)", xwav);
DECLARE_MULTIPLE_FILE_TYPE("XWB Audio File (*.XWB)", xwb);

DECLARE_MULTIPLE_FILE_TYPE("YDSP Audio File (*.YDSP)", ydsp);
DECLARE_MULTIPLE_FILE_TYPE("YMF Audio File (*.YMF)", ymf);

DECLARE_MULTIPLE_FILE_TYPE("ZSD Audio File (*.ZSD)", zsd);
DECLARE_MULTIPLE_FILE_TYPE("ZWDSP Audio File (*.ZWDSP)", zwdsp);

DECLARE_MULTIPLE_FILE_TYPE("vgmstream Audio File (*.VGMSTREAM)", vgmstream);
