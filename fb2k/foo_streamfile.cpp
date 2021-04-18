#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <io.h>

#include <foobar2000.h>
#include <ATLHelpers/ATLHelpersLean.h>
#include <shared.h>

extern "C" {
#include "../src/vgmstream.h"
#include "../src/util.h"
}
#include "foo_vgmstream.h"


/* a STREAMFILE that operates via foobar's file service using a buffer */
typedef struct {
    STREAMFILE sf;          /* callbacks */

    bool m_file_opened;         /* if foobar IO service opened the file */
    service_ptr_t<file> m_file; /* foobar IO service */
    abort_callback * p_abort;   /* foobar error stuff */
    char * name;                /* IO filename */
    off_t offset;           /* last read offset (info) */
    off_t buffer_offset;    /* current buffer data start */
    uint8_t * buffer;       /* data buffer */
    size_t buffersize;      /* max buffer size */
    size_t validsize;       /* current buffer size */
    size_t filesize;        /* buffered file size */
} FOO_STREAMFILE;

static STREAMFILE * open_foo_streamfile_buffer(const char * const filename, size_t buffersize, abort_callback * p_abort, t_filestats * stats);
static STREAMFILE * open_foo_streamfile_buffer_by_file(service_ptr_t<file> m_file, bool m_file_opened, const char * const filename, size_t buffersize, abort_callback * p_abort);

static size_t read_foo(FOO_STREAMFILE *streamfile, uint8_t * dest, off_t offset, size_t length) {
    size_t length_read_total = 0;

    if (!streamfile || !streamfile->m_file_opened || !dest || length <= 0 || offset < 0)
        return 0;

    /* is the part of the requested length in the buffer? */
    if (offset >= streamfile->buffer_offset && offset < streamfile->buffer_offset + streamfile->validsize) {
        size_t length_to_read;
        off_t offset_into_buffer = offset - streamfile->buffer_offset;

        length_to_read = streamfile->validsize - offset_into_buffer;
        if (length_to_read > length)
            length_to_read = length;

        memcpy(dest,streamfile->buffer + offset_into_buffer,length_to_read);
        length_read_total += length_to_read;
        length -= length_to_read;
        offset += length_to_read;
        dest += length_to_read;
    }


    /* read the rest of the requested length */
    while (length > 0) {
        size_t length_to_read;

        /* ignore requests at EOF */
        if (offset >= streamfile->filesize) {
            //offset = streamfile->filesize; /* seems fseek doesn't clamp offset */
            //VGM_ASSERT_ONCE(offset > streamfile->filesize, "STDIO: reading over filesize 0x%x @ 0x%lx + 0x%x\n", streamfile->filesize, offset, length);
            break;
        }

        /* position to new offset */
        try {
            streamfile->m_file->seek(offset,*streamfile->p_abort);
        } catch (...) {
            break; /* this shouldn't happen in our code */
        }

        /* fill the buffer (offset now is beyond buffer_offset) */
        try {
            streamfile->buffer_offset = offset;
            streamfile->validsize = streamfile->m_file->read(streamfile->buffer,streamfile->buffersize,*streamfile->p_abort);
        } catch(...) {
            break; /* improbable? */
        }

        /* decide how much must be read this time */
        if (length > streamfile->buffersize)
            length_to_read = streamfile->buffersize;
        else
            length_to_read = length;

        /* give up on partial reads (EOF) */
        if (streamfile->validsize < length_to_read) {
            memcpy(dest,streamfile->buffer,streamfile->validsize);
            offset += streamfile->validsize;
            length_read_total += streamfile->validsize;
            break;
        }

        /* use the new buffer */
        memcpy(dest,streamfile->buffer,length_to_read);
        offset += length_to_read;
        length_read_total += length_to_read;
        length -= length_to_read;
        dest += length_to_read;
    }

    streamfile->offset = offset; /* last fread offset */
    return length_read_total;
}
static size_t get_size_foo(FOO_STREAMFILE * streamfile) {
    return streamfile->filesize;
}
static off_t get_offset_foo(FOO_STREAMFILE *streamfile) {
    return streamfile->offset;
}
static void get_name_foo(FOO_STREAMFILE *streamfile,char *buffer,size_t length) {
   /* Most crap only cares about the filename itself */
   size_t ourlen = strlen(streamfile->name);
   if (ourlen > length) {
      if (length) strcpy(buffer, streamfile->name + ourlen - length + 1);
   } else {
      strcpy(buffer, streamfile->name);
   }
}
static void close_foo(FOO_STREAMFILE * streamfile) {
    streamfile->m_file.release(); //release alloc'ed ptr
    free(streamfile->name);
    free(streamfile->buffer);
    free(streamfile);
}

static STREAMFILE *open_foo(FOO_STREAMFILE *streamFile,const char * const filename,size_t buffersize) {
    service_ptr_t<file> m_file;

    STREAMFILE *newstreamFile;

    if (!filename)
        return NULL;

    // if same name, duplicate the file pointer we already have open
    if (streamFile->m_file_opened && !strcmp(streamFile->name,filename)) {
        m_file = streamFile->m_file; //copy?
        {
            newstreamFile = open_foo_streamfile_buffer_by_file(m_file, streamFile->m_file_opened, filename, buffersize, streamFile->p_abort);
            if (newstreamFile) {
                return newstreamFile;
            }
            // failure, close it and try the default path (which will probably fail a second time)
        }
    }

    // a normal open, open a new file
    return open_foo_streamfile_buffer(filename,buffersize,streamFile->p_abort,NULL);
}

static STREAMFILE * open_foo_streamfile_buffer_by_file(service_ptr_t<file> m_file, bool m_file_opened, const char * const filename, size_t buffersize, abort_callback * p_abort) {
    uint8_t * buffer;
    FOO_STREAMFILE * streamfile;

    buffer = (uint8_t *) calloc(buffersize,1);
    if (!buffer) goto fail;

    streamfile = (FOO_STREAMFILE *) calloc(1,sizeof(FOO_STREAMFILE));
    if (!streamfile) goto fail;

    streamfile->sf.read = (size_t (__cdecl *)(_STREAMFILE *,uint8_t *,off_t,size_t)) read_foo;
    streamfile->sf.get_size = (size_t (__cdecl *)(_STREAMFILE *)) get_size_foo;
    streamfile->sf.get_offset = (off_t (__cdecl *)(_STREAMFILE *)) get_offset_foo;
    streamfile->sf.get_name = (void (__cdecl *)(_STREAMFILE *,char *,size_t)) get_name_foo;
    streamfile->sf.open = (_STREAMFILE *(__cdecl *)(_STREAMFILE *,const char *const ,size_t)) open_foo;
    streamfile->sf.close = (void (__cdecl *)(_STREAMFILE *)) close_foo;

    streamfile->m_file_opened = m_file_opened;
    streamfile->m_file = m_file;
    streamfile->p_abort = p_abort;
    streamfile->buffersize = buffersize;
    streamfile->buffer = buffer;

    streamfile->name = strdup(filename);
    if (!streamfile->name)  goto fail;

    /* cache filesize */
    if (streamfile->m_file_opened)
        streamfile->filesize = streamfile->m_file->get_size(*streamfile->p_abort);
    else
        streamfile->filesize = 0;

    return &streamfile->sf;

fail:
    free(buffer);
    free(streamfile);
    return NULL;
}

static STREAMFILE* open_foo_streamfile_buffer(const char* const filename, size_t buffersize, abort_callback* p_abort, t_filestats* stats) {
    STREAMFILE* sf = NULL;
    service_ptr_t<file> infile;
    bool infile_exists;

    try {
        infile_exists = filesystem::g_exists(filename, *p_abort);
        if (!infile_exists) {
            /* allow non-existing files in some cases */
            if (!vgmstream_is_virtual_filename(filename))
                return NULL;
        }

        if (infile_exists) {
            filesystem::g_open_read(infile, filename, *p_abort);
            if(stats) *stats = infile->get_stats(*p_abort);
        }
        
        sf = open_foo_streamfile_buffer_by_file(infile, infile_exists, filename, buffersize, p_abort);
        if (!sf) {
            //m_file.release(); //refcounted and cleaned after it goes out of scope
        }

    } catch (...) {
        /* somehow foobar2000 throws an exception on g_exists when filename has a double \
         * (traditionally Windows treats that like a single slash and fopen handles it fine) */
        return NULL;
    }

    return sf;
}

STREAMFILE* open_foo_streamfile(const char* const filename, abort_callback* p_abort, t_filestats* stats) {
    return open_foo_streamfile_buffer(filename, STREAMFILE_DEFAULT_BUFFER_SIZE, p_abort, stats);
}
