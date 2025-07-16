/**
 * vgmstream for foobar2000
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <stdio.h>
#include <io.h>
#include <locale.h>

#include <foobar2000/SDK/foobar2000.h>

#include "foo_vgmstream.h"
#include "foo_filetypes.h"


#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif

#define PLUGIN_NAME  "vgmstream plugin"
#define PLUGIN_VERSION  VGMSTREAM_VERSION
#define PLUGIN_INFO  PLUGIN_NAME " " PLUGIN_VERSION " (" __DATE__ ")"
#define PLUGIN_DESCRIPTION  PLUGIN_INFO "\n" \
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm, Nicknine, Thealexbarney, CyberBotX, and many others\n" \
            "\n" \
            "foobar2000 plugin by Josh W, kode54, others\n" \
            "\n" \
            "https://github.com/vgmstream/vgmstream/\n" \
            "https://sourceforge.net/projects/vgmstream/ (original)"

#define PLUGIN_FILENAME "foo_input_vgmstream.dll"


static void log_callback(int level, const char* str) {
    console::formatter() /*<< "vgmstream: "*/ << str;
}

// called every time a file is added to the playlist (to get info) or when playing
input_vgmstream::input_vgmstream() {
    vgmstream = NULL;
    subsong = 0; // 0 = not set, will be properly changed on first setup_vgmstream
    direct_subsong = false;

    fade_seconds = 10.0;
    fade_delay_seconds = 0.0;
    loop_count = 2.0;
    allow_loop_forever = true;
    loop_forever = false;
    ignore_loop = false;
    disable_subsongs = false;
    downmix_channels = 0;
    tagfile_disable = false;
    tagfile_name = "!tags.m3u";
    override_title = false;

    load_settings();

    libvgmstream_set_log(LIBVGMSTREAM_LOG_LEVEL_ALL, log_callback);
}

// called on stop or when playlist info has been read
input_vgmstream::~input_vgmstream() {
    libvgmstream_free(vgmstream);
    vgmstream = NULL;
}

// called first when a new file is accepted, before playing it
void input_vgmstream::open(service_ptr_t<file> p_filehint, const char * p_path, t_input_open_reason p_reason, abort_callback & p_abort) {

    if (!p_path) // shouldn't be possible
        throw exception_io_data();

    filename = p_path;

    // allow non-existing files in some cases
    bool infile_virtual = !filesystem::g_exists(p_path, p_abort) && libvgmstream_is_virtual_filename(filename);

    // don't try to open virtual files as it'll fail
    // (doesn't seem to have any adverse effect, except maybe no stats)
    // setup_vgmstream also makes further checks before file is finally opened
    if (!infile_virtual) {
        // keep file stats around (timestamp, filesize)
        if ( p_filehint.is_empty() ) {
            input_open_file_helper( p_filehint, filename, p_reason, p_abort );
        }
        stats = p_filehint->get_stats( p_abort );

        uint32_t flags = stats2_legacy; //foobar2000_io.stats2_xxx, not sure about implications
        stats2 = p_filehint->get_stats2_(flags, p_abort); // ???
    }

    switch(p_reason) {
        case input_open_decode:         // prepare to retrieve info and decode
        case input_open_info_read:      // prepare to retrieve info
            // init vgmstream to get subsongs
            setup_vgmstream(p_abort);
            break;

        case input_open_info_write:     // prepare to retrieve info and tag
        default:
            throw exception_io_data();
    }
}

// called after opening file (possibly per subsong too)
unsigned input_vgmstream::get_subsong_count() {
    if (disable_subsongs)
        return 1;
    // If the plugin uses input_factory_t template and returns > 1 here when adding a song to the playlist,
    // foobar will automagically "unpack" it by calling decode_initialize/get_info with all subsong indexes.
    // There is no need to add any playlist code, only properly handle the subsong index.

    // vgmstream ready as method is valid after open() with any reason
    int subsong_count = vgmstream->format->subsong_count;
    if (subsong_count == 0)
        subsong_count = 1; // most formats don't have subsongs

    // pretend we don't have subsongs as we don't want foobar to unpack the rest
    if (direct_subsong)
        subsong_count = 1;

    return subsong_count;
}

// called after get_subsong_count to play subsong N (even when count is 1)
t_uint32 input_vgmstream::get_subsong(unsigned p_index) {
    // translates index (0..N < subsong_count) for vgmstream: 1=first
    return p_index + 1;
}

// called before playing to get info
void input_vgmstream::get_info(t_uint32 p_subsong, file_info& p_info, abort_callback& p_abort) {
    vgmstream_info_t v_info = {}; // init default (not {0} since it has classes)

    query_subsong_info(p_subsong, v_info, p_abort);

    // export tags ('metadata' tab in file properties)
    put_info_tags(p_info, v_info);
    put_into_tagfile(p_info, p_abort);

    // set technical info ('details' tab in file properties)
    put_info_details(p_info, v_info);
}

void input_vgmstream::put_info_tags(file_info& p_info, vgmstream_info_t& v_info) {
    if (!override_title) {
        /* Shows a default (sub)song title with stream name.
         * This can be overriden and extended using the exported STREAM_x below and foobar's formatting.
         * foobar defaults to filename minus extension if there is no meta "title" value. */
        p_info.meta_set("TITLE", v_info.title);
    }

    if (!v_info.stream_name.is_empty()) {
        p_info.meta_set("stream_name", v_info.stream_name);
    }

    if (v_info.subsong_count > 1) {
        p_info.meta_set("stream_count", pfc::format_int(v_info.subsong_count));
        p_info.meta_set("stream_index", pfc::format_int(v_info.subsong_index == 0 ? 1 : v_info.subsong_index));
    }

    if (v_info.loop_end > 0) {
        p_info.meta_set("loop_start", pfc::format_int(v_info.loop_start));
        p_info.meta_set("loop_end", pfc::format_int(v_info.loop_end));
    }
}

void input_vgmstream::put_into_tagfile(file_info& p_info, abort_callback& p_abort) {
    if (tagfile_disable)
        return;
    //TODO: optimize and don't parse tags again for this session (not sure how).
    // Seems foobar calls get_info on every play even if the file hasn't changed,
    // and won't refresh "meta" unless forced or closing playlist + exe.

    //TODO: use foobar's fancy-but-arcane string functions
    char tagfile_path[FOO_PATH_LIMIT];
    strcpy(tagfile_path, filename);

    char* path = strrchr(tagfile_path, '\\');
    if (path != NULL) {
        path[1] = '\0';  // includes "\", remove after that from tagfile_path
        strcat(tagfile_path, tagfile_name);
    }
    else {
        // possible?
        strcpy(tagfile_path, tagfile_name);
    }
    

    libstreamfile_t* sf_tags = open_foo_streamfile(tagfile_path, &p_abort, NULL);
    if (sf_tags == NULL)
        return;
    
    libvgmstream_tags_t* tags = libvgmstream_tags_init(sf_tags);

    libvgmstream_tags_find(tags, filename);
    while (libvgmstream_tags_next_tag(tags)) {
        if (replaygain_info::g_is_meta_replaygain(tags->key)) {
            p_info.info_set_replaygain(tags->key, tags->val);
            // there is set_replaygain_auto/set_replaygain_ex too but no doc
        }
        else if (stricmp_utf8("ALBUMARTIST", tags->key) == 0) {
            // normalize tag for foobar as otherwise can't understand it (though it's recognized in .ogg)
            p_info.meta_set("ALBUM ARTIST", tags->val);
        }
        else {
            p_info.meta_set(tags->key, tags->val);
        }
    }

    libvgmstream_tags_free(tags);
    libstreamfile_close(sf_tags);
}

// include main info, note that order doesn't matter (foobar sorts by fixed order + name)
void input_vgmstream::put_info_details(file_info& p_info, vgmstream_info_t& v_info) {
    p_info.info_set("vgmstream_version", PLUGIN_VERSION); //to make clearer vgmstream is actually opening the file

    // not quite accurate but some people are confused by "lossless"
    // (could set lossless if PCM, but then again in vgm may be converted/"lossy" vs original source)
    p_info.info_set("encoding", "lossy/lossless");

    p_info.info_set_int("channels", v_info.channels);
    p_info.info_set_int("samplerate", v_info.sample_rate);
    p_info.info_set_bitrate(v_info.bitrate / 1000);

    // "32-bit" float seems to confuse people, foobar's Ogg decoder doesn't set this either.
    if (vgmstream->format->sample_format != LIBVGMSTREAM_SFMT_FLOAT) {
        p_info.info_set_int("bitspersample", v_info.bits_per_sample);
    }

    if (v_info.input_channels > 0 && v_info.channels != v_info.input_channels) {
        p_info.info_set_int("input_channels", v_info.input_channels);
        //p_info.info_set_int("output_channels", v_info.output_channels);
    }

    p_info.set_length(v_info.play_length_s);

    if (v_info.stream_samples > 0) { // ?
        p_info.info_set_int("samples", v_info.stream_samples);
    }

    if (v_info.loop_end > 0) {
        p_info.info_set_int("loop_start", v_info.loop_start);
        p_info.info_set_int("loop_end", v_info.loop_end);
        if (!v_info.loop_flag)
            p_info.info_set("looping", "disabled");
    }

    p_info.info_set("codec", v_info.codec_name);
    p_info.info_set("layout", v_info.layout_name);
    p_info.info_set("metadata", v_info.meta_name);

    if (!v_info.stream_name.is_empty()) {
        p_info.info_set("stream_name", v_info.stream_name);
    }

    if (v_info.subsong_count > 1) {
        p_info.info_set_int("stream_count", v_info.subsong_count);
        p_info.info_set_int("stream_index", v_info.subsong_index);
    }

    /*
    if (!v_info.channel_mask.is_empty()) {
        p_info.info_set("channel_mask", v_info.channel_mask);
    }
    */

    /*
    // for >2ch foobar writes info like "Channels - 8: FL FR FC LFE BL BR FCL FCR", which may be undesirable?
    if (v_info.channel_layout > 0) {
        // there is info_set_channels_ex in newer SDKs too
        p_info.info_set_wfx_chanMask(v_info.channel_layout);
    }
    */

    //if (v_info.interleave > 0) p_info.info_set("interleave", v_info.interleave);
    //if (v_info.interleave_last > 0) p_info.info_set("interleave_last_block", v_info.interleave_last);
    //if (v_info.block_size > 0) p_info.info_set("block_size", v_info.block_size);
}

t_filestats input_vgmstream::get_file_stats(abort_callback & p_abort) {
    return stats;
}

t_filestats2 input_vgmstream::get_stats2(uint32_t f, abort_callback & p_abort) {
    return stats2;
}

// called right before actually playing (decoding) a song/subsong
void input_vgmstream::decode_initialize(t_uint32 p_subsong, unsigned p_flags, abort_callback & p_abort) {

    // "don't loop forever" flag (set when converting to file, scanning for replaygain, etc)
    bool force_ignore_loop = !!(p_flags & input_flag_no_looping);
    if (force_ignore_loop)
        allow_loop_forever = false;

    //;console::formatter() << "allow: " << allow_loop_forever;

    // if subsong changes recreate vgmstream
    if (subsong != p_subsong && !direct_subsong || force_ignore_loop) {
        subsong = p_subsong;
        //;console::formatter() << "setup: ";
        setup_vgmstream(p_abort);
    }

    //decode_seek(0, p_abort);
};

// called when audio buffer needs to be filled, returns false on EOF
bool input_vgmstream::decode_run(audio_chunk & p_chunk, abort_callback & p_abort) {
    if (!vgmstream)
        return false;

    if (vgmstream->decoder->done)
        return false; // EOF

    int calls = 0;
    while (true) {
        int err = libvgmstream_render(vgmstream);
        if (err < 0) return false;

        // vgmstream may return 0 on rare cases, but foobar/p_chunk can't handle this and will
        // throw an error (it handles bufs of few samples so it could be considered a bug on their part).
        // 'return true' without updating p_chunk will result on repeated samples (re-reads last p_chunk).
        if (vgmstream->decoder->buf_samples != 0)
            break;

        // 0 samples and done = EOF
        if (vgmstream->decoder->done) 
            return false;

        // a few 0s may be normal
        calls++;
        if (calls > 1000) {
            console::formatter() << "vgmstream render deadlock found";
            return false;
        }
    }

    int sample_rate = vgmstream->format->sample_rate;
    int channels = vgmstream->format->channels;
    unsigned channel_config = vgmstream->format->channel_layout;
    if (!channel_config)
        channel_config = audio_chunk::g_guess_channel_config(channels);

    int bps = vgmstream->format->sample_size * 8;
    void* buf = vgmstream->decoder->buf;
    t_size bytes = vgmstream->decoder->buf_bytes;

    switch (vgmstream->format->sample_format) {
        case LIBVGMSTREAM_SFMT_FLOAT:
            p_chunk.set_data_floatingpoint_ex(buf, bytes, sample_rate, channels, bps, 0, channel_config);
            break;
        case LIBVGMSTREAM_SFMT_PCM16:
        case LIBVGMSTREAM_SFMT_PCM24:
        case LIBVGMSTREAM_SFMT_PCM32:
            p_chunk.set_data_fixedpoint(buf, bytes, sample_rate, channels, bps, channel_config);
            break;
        default:
            break;
    }

    return true; // decoded and p_chunk is properly set
}

// called when seeking
void input_vgmstream::decode_seek(double p_seconds, abort_callback& p_abort) {
    int64_t seek_sample = (int64_t)audio_math::time_to_samples(p_seconds, vgmstream->format->sample_rate);
    bool play_forever = vgmstream->format->play_forever;

    // TODO: check play position after seek_sample and let seek clamp
    // possible when disabling looping without refreshing foobar's cached song length
    // (p_seconds can't go over seek bar with infinite looping on, though)
    int64_t play_samples = vgmstream->format->play_samples;
    if (seek_sample > play_samples)
        seek_sample = play_samples;

    libvgmstream_seek(vgmstream, seek_sample);
}

bool input_vgmstream::decode_can_seek() { return true; }
bool input_vgmstream::decode_get_dynamic_info(file_info& p_out, double& p_timestamp_delta) { return false; }
bool input_vgmstream::decode_get_dynamic_info_track(file_info& p_out, double& p_timestamp_delta) { return false; }
void input_vgmstream::decode_on_idle(abort_callback& p_abort) { /*m_file->on_idle(p_abort);*/ }

void input_vgmstream::retag_set_info(t_uint32 p_subsong, const file_info& p_info, abort_callback& p_abort) { /*throw exception_io_data();*/ }
void input_vgmstream::retag_commit(abort_callback& p_abort) { /*throw exception_io_data();*/ }
void input_vgmstream::remove_tags(abort_callback& p_abort) { /*throw exception_io_data();*/ }

bool input_vgmstream::g_is_our_content_type(const char* p_content_type) { return false; }

// called to check if file can be processed by the plugin
bool input_vgmstream::g_is_our_path(const char* p_path, const char* p_extension) {
    libvgmstream_valid_t vcfg = {0};

    vcfg.is_extension = true;
    input_vgmstream::g_load_cfg(&vcfg.accept_unknown, &vcfg.accept_common);
    return libvgmstream_is_valid(p_extension, &vcfg);
}


// internal util to create a VGMSTREAM
libvgmstream_t* input_vgmstream::init_vgmstream_foo(t_uint32 p_subsong, const char* const filename, abort_callback& p_abort) {

    /* Workaround for a foobar bug (mainly for complex TXTP):
     * When converting to .ogg foobar calls oggenc, that calls setlocale(LC_ALL, "") to use system's locale.
     * After that, text parsing using sscanf that expects US locale for "N.N" decimals fails in some locales,
     * so reset it here just in case
     * (maybe should be done on lib and/or restore original locale but it's not common to change it in C) */
    //const char* original_locale = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "C");


    libvgmstream_config_t vcfg = {0};
    load_vconfig(&vcfg);

    libstreamfile_t* sf = open_foo_streamfile(filename, &p_abort, &stats);
    if (!sf) return NULL;

    libvgmstream_t* vgmstream = libvgmstream_create(sf, p_subsong, &vcfg);
    if (!vgmstream) {
        libvgmstream_free(vgmstream);
        libstreamfile_close(sf);
        return NULL;
    }

    libstreamfile_close(sf);

    //setlocale(LC_ALL, original_locale);
    return vgmstream;
}

// internal util to initialize vgmstream
void input_vgmstream::setup_vgmstream(abort_callback & p_abort) {
    // close first in case of changing subsongs
    if (vgmstream) {
        libvgmstream_free(vgmstream);
    }

    // subsong and filename are always defined before this
    vgmstream = init_vgmstream_foo(subsong, filename, p_abort);
    if (!vgmstream)
        throw exception_io_data();

    // default subsong is 0, meaning first init (vgmstream should open first stream, but not set stream_index).
    // if the stream_index is already set, then the subsong was opened directly by some means (txtp, playlist, etc).
    // add flag as in that case we only want to play the subsong but not unpack subsongs (which foobar does by default).
    if (subsong == 0 && vgmstream->format->subsong_index > 0) {
        subsong = vgmstream->format->subsong_index;
        direct_subsong = true;
    }
    if (subsong == 0)
        subsong = 1;
}

void input_vgmstream::load_vconfig(libvgmstream_config_t* vcfg) {

    vcfg->allow_play_forever = allow_loop_forever;
    vcfg->play_forever = loop_forever;
    vcfg->loop_count = loop_count;
    vcfg->fade_time = fade_seconds;
    vcfg->fade_delay = fade_delay_seconds;
    vcfg->ignore_loop = ignore_loop;

    vcfg->auto_downmix_channels = downmix_channels;
}

// internal util to get info
void input_vgmstream::query_subsong_info(t_uint32 p_subsong, vgmstream_info_t& v_info, abort_callback& p_abort) {
    libvgmstream_t* infostream = NULL;
    bool is_infostream = false;

    // Reuse current vgmstream if not querying a new subsong.
    // If it's a direct subsong then subsong may be N while p_subsong = 1
    // so there is no need to recreate the infostream, only one subsong is used.
    if (subsong != p_subsong && !direct_subsong) {
        infostream = init_vgmstream_foo(p_subsong, filename, p_abort);
        if (!infostream)
            throw exception_io_data();

        is_infostream = true;
    }
    else {
        // vgmstream ready as get_info is valid after open() with any reason
        infostream = vgmstream;
    }

    if (!infostream)
        throw exception_io_data();

    /* basic info */
    {
        v_info.channels = infostream->format->channels;
        v_info.input_channels = infostream->format->input_channels;
        v_info.sample_rate = infostream->format->sample_rate;
        v_info.stream_samples = infostream->format->stream_samples;
        v_info.bitrate = infostream->format->stream_bitrate;
        v_info.loop_flag = infostream->format->loop_flag;
        v_info.loop_start = infostream->format->loop_start;
        v_info.loop_end = infostream->format->loop_end;

        v_info.subsong_count = infostream->format->subsong_count;
        v_info.subsong_index = infostream->format->subsong_index;
        if (v_info.subsong_index == 0)
            v_info.subsong_index = 1;

        v_info.channel_layout = infostream->format->channel_layout;

        int64_t play_samples = infostream->format->play_samples;
        v_info.play_length_s = (double)play_samples / (double)infostream->format->sample_rate;

        v_info.bits_per_sample = infostream->format->sample_size * 8;
    }

    // formatted info
    {
        pfc::string8 description;

        v_info.codec_name = infostream->format->codec_name;
        v_info.layout_name = infostream->format->layout_name;
        v_info.meta_name = infostream->format->meta_name;
        v_info.stream_name = infostream->format->stream_name;
        //v_info.channel_mask = infostream->format->channel_mask;
    }

    // infostream gets added with index 0 (other) or 1 (current)
    {
        char temp[1024];
        libvgmstream_title_t tcfg = {0};
        tcfg.remove_extension = true;
        tcfg.remove_archive = true;
        tcfg.filename = filename;

        libvgmstream_get_title(infostream, &tcfg, temp, sizeof(temp));
        v_info.title = temp;
    }

    // and only close if was querying a new subsong
    if (is_infostream) {
        libvgmstream_free(infostream);
        infostream = NULL;
    }
}

// extract a "tag" from the description string
bool input_vgmstream::query_description_tag(pfc::string_base& tag_value, pfc::string_base const& description, const char* tag_key, char delimiter) {

    t_size pos = description.find_first(tag_key);
    if (pos == pfc::infinite_size)
        return false;
    pos += strlen(tag_key);

    t_size eos = description.find_first(delimiter, pos);
    if (eos == pfc::infinite_size)
        eos = description.length();

    tag_value.set_string(description + pos, eos - pos);
    return true;
}


// checks priority (foobar 1.4+)
bool input_vgmstream::g_is_low_merit() {
    return true;
}

// foobar recognizes plugin with this (meaning, different GUID = different plugin)
GUID input_vgmstream::g_get_guid() {
    static const GUID guid = { 0x9e7263c7, 0x4cdd, 0x482c,{ 0x9a, 0xec, 0x5e, 0x71, 0x28, 0xcb, 0xc3, 0x4 } };
    return guid;
}

GUID input_vgmstream::g_get_preferences_guid() {
    static const GUID guid = { 0x2b5d0302, 0x165b, 0x409c,{ 0x94, 0x74, 0x2c, 0x8c, 0x2c, 0xd7, 0x6a, 0x25 } };
    return guid;
}

const char* input_vgmstream::g_get_name() {
    return "vgmstream";
}

// foobar plugin defs
static input_factory_t<input_vgmstream> g_input_vgmstream_factory;

DECLARE_COMPONENT_VERSION(PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_DESCRIPTION);
VALIDATE_COMPONENT_FILENAME(PLUGIN_FILENAME);
