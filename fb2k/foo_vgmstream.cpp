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


input_vgmstream::input_vgmstream() {
    vgmstream = NULL;
    subsong = 1;

    decoding = false;
    paused = 0;
    decode_pos_ms = 0;
    decode_pos_samples = 0;
    stream_length_samples = 0;
    fade_samples = 0;
    seek_pos_samples = 0;

    fade_seconds = 10.0f;
    fade_delay_seconds = 0.0f;
    loop_count = 2.0f;
    loop_forever = false;
    ignore_loop = 0;
    disable_subsongs = true;

    load_settings();
}

input_vgmstream::~input_vgmstream() {
    close_vgmstream(vgmstream);
    vgmstream = NULL;
}

void input_vgmstream::open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort) {

    if (!p_path) { // shouldn't be possible
        throw exception_io_data();
        return;
    }

    filename = p_path;

    /* KLUDGE */
    if ( !pfc::stricmp_ascii( pfc::string_extension(filename), "MUS" ) )
    {
        unsigned char buffer[ 4 ];
        if ( p_filehint.is_empty() ) input_open_file_helper( p_filehint, filename, p_reason, p_abort );
        p_filehint->read_object_t( buffer, p_abort );
        if ( !memcmp( buffer, "MUS\x1A", 4 ) ) throw exception_io_unsupported_format();
    }


    // keep file stats around (timestamp, filesize)
    if ( p_filehint.is_empty() )
        input_open_file_helper( p_filehint, filename, p_reason, p_abort );
    stats = p_filehint->get_stats( p_abort );

    switch(p_reason) {
        case input_open_decode: // prepare to retrieve info and decode
        case input_open_info_read: // prepare to retrieve info
            setup_vgmstream(p_abort); // must init vgmstream to get subsongs
            break;

        case input_open_info_write: // prepare to retrieve info and tag
            throw exception_io_data();
            break;

        default: // nothing else should be possible
            throw exception_io_data();
            break;
    }
}

unsigned input_vgmstream::get_subsong_count() {
    // if the plugin uses input_factory_t template and returns > 1 here when adding a song to the playlist,
    // foobar will automagically "unpack" it by calling decode_initialize/get_info with all subsong indexes.
    // There is no need to add any playlist code, only properly handle the subsong index.
    if (disable_subsongs)
        return 1;

    // vgmstream ready as method is valid after open() with any reason
    int subsong_count = vgmstream->num_streams;
    if (subsong_count == 0)
        subsong_count = 1; // most formats don't have subsongs
    return subsong_count;
}

t_uint32 input_vgmstream::get_subsong(unsigned p_index) {
    return p_index + 1; // translates index (0..N < subsong_count) for vgmstream: 1=first
}

void input_vgmstream::get_info(t_uint32 p_subsong, file_info & p_info, abort_callback & p_abort) {
    int length_in_ms=0, channels = 0, samplerate = 0;
    int total_samples = -1;
    int bitrate = 0;
    int loop_start = -1, loop_end = -1;
    pfc::string8 description;
    pfc::string8_fast temp;

    get_subsong_info(p_subsong, NULL, &length_in_ms, &total_samples, &loop_start, &loop_end, &samplerate, &channels, &bitrate, description, p_abort);

    p_info.info_set_int("samplerate", samplerate);
    p_info.info_set_int("channels", channels);
    p_info.info_set_int("bitspersample",16);
    p_info.info_set("encoding","lossless");
    p_info.info_set_bitrate(bitrate / 1000);
    if (total_samples > 0)
        p_info.info_set_int("stream_total_samples", total_samples);
    if (loop_start >= 0 && loop_end >= loop_start) {
        p_info.info_set_int("loop_start", loop_start);
        p_info.info_set_int("loop_end", loop_end);
    }
    p_info.set_length(((double)length_in_ms)/1000);

    if (get_description_tag(temp,description,"encoding: ")) p_info.info_set("codec",temp);
    if (get_description_tag(temp,description,"layout: ")) p_info.info_set("layout",temp);
    if (get_description_tag(temp,description,"interleave: ",' ')) p_info.info_set("interleave",temp);
    if (get_description_tag(temp,description,"last block interleave:",' ')) p_info.info_set("interleave_last_block",temp);

    if (get_description_tag(temp,description,"block size: ")) p_info.info_set("block_size",temp);
    if (get_description_tag(temp,description,"metadata from: ")) p_info.info_set("metadata_source",temp);
    if (get_description_tag(temp,description,"stream number: ")) p_info.info_set("stream_number",temp);
    if (get_description_tag(temp,description,"stream index: ")) p_info.info_set("stream_index",temp);
    if (get_description_tag(temp,description,"stream name: ")) p_info.info_set("stream_name",temp);
}

t_filestats input_vgmstream::get_file_stats(abort_callback & p_abort) {
    return stats;
}


void input_vgmstream::decode_initialize(t_uint32 p_subsong, unsigned p_flags, abort_callback & p_abort) {
    force_ignore_loop = !!(p_flags & input_flag_no_looping);

    // if subsong changes recreate vgmstream (subsong is only used on init)
    if (subsong != p_subsong) {
        subsong = p_subsong;
        setup_vgmstream(p_abort);
    }

    decode_seek( 0, p_abort );
};

bool input_vgmstream::decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
    if (!decoding) return false;

    int max_buffer_samples = sizeof(sample_buffer)/sizeof(sample_buffer[0])/vgmstream->channels;
    int l = 0, samples_to_do = max_buffer_samples;

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

bool input_vgmstream::decode_can_seek() {return true;}
bool input_vgmstream::decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) { return true; }
bool input_vgmstream::decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return false;}
void input_vgmstream::decode_on_idle(abort_callback & p_abort) {/*m_file->on_idle(p_abort);*/}

void input_vgmstream::retag_set_info(t_uint32 p_subsong, const file_info & p_info, abort_callback & p_abort) { /*throw exception_io_data();*/ }
void input_vgmstream::retag_commit(abort_callback & p_abort) { /*throw exception_io_data();*/ }

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


VGMSTREAM * input_vgmstream::init_vgmstream_foo(t_uint32 p_subsong, const char * const filename, abort_callback & p_abort) {
    VGMSTREAM *vgmstream = NULL;

    STREAMFILE *streamFile = open_foo_streamfile(filename, &p_abort, &stats);
    if (streamFile) {
        streamFile->stream_index = p_subsong;
        vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
        close_streamfile(streamFile);
    }
    return vgmstream;
}

void input_vgmstream::setup_vgmstream(abort_callback & p_abort) {
    // close first in case of changing subsongs
    if (vgmstream) {
        close_vgmstream(vgmstream);
    }

    // subsong and filename are always defined before this
    vgmstream = init_vgmstream_foo(subsong, filename, p_abort);
    if (!vgmstream) {
        throw exception_io_data();
        return;
    }

    if (ignore_loop)
        vgmstream->loop_flag = 0;

    decode_pos_ms = 0;
    decode_pos_samples = 0;
    paused = 0;
    stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);

    fade_samples = (int)(fade_seconds * vgmstream->sample_rate);
}

void input_vgmstream::get_subsong_info(t_uint32 p_subsong, char *title, int *length_in_ms, int *total_samples, int *loop_start, int *loop_end, int *sample_rate, int *channels, int *bitrate, pfc::string_base & description, abort_callback & p_abort) {
    VGMSTREAM * infostream = NULL;

    // reuse current vgmstream if not querying a new subsong
    if (subsong != p_subsong) {
        infostream = init_vgmstream_foo(p_subsong, filename, p_abort);
    } else {
        // vgmstream ready as get_info is valid after open() with any reason
        infostream = vgmstream;
    }


    if (length_in_ms) {
        *length_in_ms = -1000;
        if (infostream) {
            *length_in_ms = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,infostream)*1000LL/infostream->sample_rate;
            *sample_rate = infostream->sample_rate;
            *channels = infostream->channels;
            *total_samples = infostream->num_samples;
            *bitrate = get_vgmstream_average_bitrate(infostream);
            if (infostream->loop_flag) {
                *loop_start = infostream->loop_start_sample;
                *loop_end = infostream->loop_end_sample;
            }

            char temp[1024];
            describe_vgmstream(infostream, temp, 1024);
            description = temp;
        }
    }

    if (title) {
        const char *p=filename+strlen(filename);
        while (*p != '\\' && p >= filename) p--;
        strcpy(title,++p);
    }

    // and only close if was querying a new subsong
    if (subsong != p_subsong) {
        close_vgmstream(infostream);
        infostream = NULL;
    }
}

bool input_vgmstream::get_description_tag(pfc::string_base & temp, pfc::string_base & description, const char *tag, char delimiter) {
    // extract a "tag" from the description string
    t_size pos = description.find_first(tag);
    t_size eos;
    if (pos != pfc::infinite_size) {
        pos += strlen(tag);
        eos = description.find_first(delimiter, pos);
        if (eos == pfc::infinite_size) eos = description.length();
        temp.set_string(description + pos, eos - pos);
        return true;
    }
    return false;
}


/* foobar plugin defs */
static input_factory_t<input_vgmstream> g_input_vgmstream_factory;

DECLARE_COMPONENT_VERSION(APP_NAME,PLUGIN_VERSION,PLUGIN_DESCRIPTION);
VALIDATE_COMPONENT_FILENAME("foo_input_vgmstream.dll");
