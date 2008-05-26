#ifndef _FOO_VGMSTREAM_
#define _FOO_VGMSTREAM_

typedef struct _FOO_STREAMFILE {
	struct _STREAMFILE sf;
	abort_callback * p_abort;
	service_ptr_t<file> m_file;
	char name[260];
	off_t offset;
    size_t validsize;
    uint8_t * buffer;
    size_t buffersize;
}  FOO_STREAMFILE;


static STREAMFILE * open_foo_streamfile_buffer_by_file(service_ptr_t<file> m_file,const char * const filename, size_t buffersize, abort_callback * p_abort);
STREAMFILE * open_foo_streamfile_buffer(const char * const filename, size_t buffersize, abort_callback * p_abort);
STREAMFILE * open_foo_streamfile(const char * const filename, abort_callback * p_abort);

#define DECLARE_MULTIPLE_FILE_TYPE(NAME,EXTENSION) \
	namespace { static input_file_type_impl g_filetype_instance_##EXTENSION(NAME,"*." #EXTENSION ,true); \
	static service_factory_single_ref_t<input_file_type_impl> g_filetype_service##EXTENSION(g_filetype_instance_##EXTENSION); }


#endif
