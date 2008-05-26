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


static size_t read_the_rest_foo(uint8_t * dest, off_t offset, size_t length, FOO_STREAMFILE * streamfile) {
    size_t length_read_total=0;

    /* is the beginning at least there? */
    if (offset >= streamfile->offset && offset < streamfile->offset+streamfile->validsize) {
        size_t length_read;
        off_t offset_into_buffer = offset-streamfile->offset;
        length_read = streamfile->validsize-offset_into_buffer;
        memcpy(dest,streamfile->buffer+offset_into_buffer,length_read);
        length_read_total += length_read;
        length -= length_read;
        offset += length_read;
        dest += length_read;
    }

    /* TODO: What would make more sense here is to read the whole request
     * at once into the dest buffer, as it must be large enough, and then
     * copy some part of that into our own buffer.
     * The destination buffer is supposed to be much smaller than the
     * STREAMFILE buffer, though. Maybe we should only ever return up
     * to the buffer size to avoid having to deal with things like this
     * which are outside of my intended use.
     */
    /* read as much of the beginning of the request as possible, proceed */
    while (length>0) {
        size_t length_to_read;
        size_t length_read=0;
        streamfile->validsize=0;

        streamfile->m_file->seek(offset,*streamfile->p_abort);
        if (streamfile->m_file->is_eof(*streamfile->p_abort)) return length_read;

        streamfile->offset=offset;

        /* decide how much must be read this time */
        if (length>streamfile->buffersize) length_to_read=streamfile->buffersize;
        else length_to_read=length;

        /* always try to fill the buffer */
        length_read = streamfile->m_file->read(streamfile->buffer,streamfile->buffersize,*streamfile->p_abort);
        streamfile->validsize=length_read;

#ifdef PROFILE_STREAMFILE
        streamfile->bytes_read += length_read;
#endif

        /* if we can't get enough to satisfy the request we give up */
        if (length_read < length_to_read) {
            memcpy(dest,streamfile->buffer,length_read);
            length_read_total+=length_read;
            return length_read_total;
        }

        /* use the new buffer */
        memcpy(dest,streamfile->buffer,length_to_read);
        length_read_total+=length_to_read;
        length-=length_to_read;
        dest+=length_to_read;
        offset+=length_to_read;
    }

    return length_read_total;
}

static size_t read_foo(FOO_STREAMFILE *streamfile, uint8_t * dest, off_t offset, size_t length) {

	if (!streamfile || !dest || length<=0) return 0;

    /* if entire request is within the buffer */
    if (offset >= streamfile->offset && offset+length <= streamfile->offset+streamfile->validsize) {
        memcpy(dest,streamfile->buffer+(offset-streamfile->offset),length);
        return length;
    }

    return read_the_rest_foo(dest,offset,length,streamfile);
}

STREAMFILE * open_foo_streamfile(const char * const filename, abort_callback * p_abort) {
	return open_foo_streamfile_buffer(filename,STREAMFILE_DEFAULT_BUFFER_SIZE, p_abort);
}

static STREAMFILE *open_foo(FOO_STREAMFILE *streamFile,const char * const filename,size_t buffersize) {
	service_ptr_t<file> m_file;

	STREAMFILE *newstreamFile;

    if (!filename)
        return NULL;

    // if same name, duplicate the file pointer we already have open
    if (!strcmp(streamFile->name,filename)) {
        m_file = streamFile->m_file;
        if (1) {
            newstreamFile = open_foo_streamfile_buffer_by_file(m_file,filename,buffersize,streamFile->p_abort);
            if (newstreamFile) {
                return newstreamFile;
            }
            // failure, close it and try the default path (which will probably fail a second time)
        }
    }
    // a normal open, open a new file

	return open_foo_streamfile_buffer(filename,buffersize,streamFile->p_abort);
}

static size_t get_size_foo(FOO_STREAMFILE * streamfile) {
    //fseeko(streamfile->infile,0,SEEK_END);
    return streamfile->m_file->get_size(*streamfile->p_abort);//ftello(streamfile->infile);
}

static off_t get_offset_foo(FOO_STREAMFILE *streamFile) {
    return streamFile->offset;
}

static void close_foo(FOO_STREAMFILE * streamfile) {
    //fclose(streamfile->infile);
    free(streamfile->buffer);
    free(streamfile);
}

static void get_name_foo(FOO_STREAMFILE *streamfile,char *buffer,size_t length) {
   strcpy(buffer,streamfile->name);
}

static STREAMFILE * open_foo_streamfile_buffer_by_file(service_ptr_t<file> m_file,const char * const filename, size_t buffersize, abort_callback * p_abort) {
    uint8_t * buffer;
    FOO_STREAMFILE * streamfile;

    buffer = (uint8_t *) calloc(buffersize,1);
    if (!buffer) {
        return NULL;
    }

    streamfile = (FOO_STREAMFILE *) calloc(1,sizeof(FOO_STREAMFILE));
    if (!streamfile) {
        free(buffer);
        return NULL;
    }

    streamfile->sf.read = (size_t (__cdecl *)(_STREAMFILE *,uint8_t *,off_t,size_t)) read_foo;
    streamfile->sf.get_size = (size_t (__cdecl *)(_STREAMFILE *)) get_size_foo;
    streamfile->sf.get_offset = (off_t (__cdecl *)(_STREAMFILE *)) get_offset_foo;
    streamfile->sf.get_name = (void (__cdecl *)(_STREAMFILE *,char *,size_t)) get_name_foo;
    streamfile->sf.open = (_STREAMFILE *(__cdecl *)(_STREAMFILE *,const char *const ,size_t)) open_foo;
    streamfile->sf.close = (void (__cdecl *)(_STREAMFILE *)) close_foo;
#ifdef PROFILE_STREAMFILE
    streamfile->sf.get_bytes_read = (void*)get_bytes_read_stdio;
#endif

    streamfile->m_file = m_file;

    streamfile->buffersize = buffersize;
    streamfile->buffer = buffer;
    streamfile->p_abort = p_abort;

    strcpy(streamfile->name,filename);

    return &streamfile->sf;
}

STREAMFILE * open_foo_streamfile_buffer(const char * const filename, size_t buffersize, abort_callback * p_abort) {
    STREAMFILE *streamFile;
    service_ptr_t<file> infile;

    if(!(filesystem::g_exists(filename, *p_abort)))
    	return NULL;

    filesystem::g_open_read(infile,filename,*p_abort);

    streamFile = open_foo_streamfile_buffer_by_file(infile,filename,buffersize,p_abort);
    if (!streamFile) {
        // fclose(infile);
    }

    return streamFile;
}
