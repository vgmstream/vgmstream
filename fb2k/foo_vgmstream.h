#ifndef _FOO_VGMSTREAM_
#define _FOO_VGMSTREAM_

#define OUTBUF_SIZE     1024        /* Samples */

class input_vgmstream {
    public:
        input_vgmstream();
        ~input_vgmstream();

        void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort);

        unsigned get_subsong_count();
        t_uint32 get_subsong(unsigned p_index);
        void get_info(t_uint32 p_subsong,file_info & p_info,abort_callback & p_abort);
        t_filestats get_file_stats(abort_callback & p_abort);

        void decode_initialize(t_uint32 p_subsong,unsigned p_flags,abort_callback & p_abort);
        bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort);
        void decode_seek(double p_seconds,abort_callback & p_abort);
        bool decode_can_seek();
        bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta);
        bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta);
        void decode_on_idle(abort_callback & p_abort);

        void retag_set_info(t_uint32 p_subsong,const file_info & p_info,abort_callback & p_abort);
        void retag_commit(abort_callback & p_abort);

        static bool g_is_our_content_type(const char * p_content_type);
        static bool g_is_our_path(const char * p_path,const char * p_extension);

    private:
        //service_ptr_t<file> m_file;
        pfc::string8 filename;
        t_filestats stats;

        /* state */
        VGMSTREAM * vgmstream;
        t_uint32 subsong;

        bool decoding;
        int paused;
        int decode_pos_ms;
        int decode_pos_samples;
        int stream_length_samples;
        int fade_samples;
        int seek_pos_samples;
        short sample_buffer[OUTBUF_SIZE];

        /* config */
        double fade_seconds;
        double fade_delay_seconds;
        double loop_count;
        bool loop_forever;
        bool force_ignore_loop;
        int ignore_loop;
        bool disable_subsongs;

        /* helpers */
        VGMSTREAM * init_vgmstream_foo(t_uint32 p_subsong, const char * const filename, abort_callback & p_abort);
        void setup_vgmstream(abort_callback & p_abort);
        void load_settings();
        void get_subsong_info(t_uint32 p_subsong, char *title, int *length_in_ms, int *total_samples, int *loop_start, int *loop_end, int *sample_rate, int *channels, int *bitrate, pfc::string_base & description, abort_callback & p_abort);
        bool get_description_tag(pfc::string_base & temp, pfc::string_base & description, const char *tag, char delimiter = '\n');
};

/* foo_streamfile.cpp */
STREAMFILE * open_foo_streamfile(const char * const filename, abort_callback * p_abort, t_filestats * stats);


#endif /*_FOO_VGMSTREAM_*/
