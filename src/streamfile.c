#ifndef _MSC_VER
#include <unistd.h>
#endif
#include "streamfile.h"
#include "util.h"
#include "vgmstream.h"


/* a STREAMFILE that operates via standard IO using a buffer */
typedef struct {
    STREAMFILE sf;          /* callbacks */

    FILE * infile;          /* actual FILE */
    char name[PATH_LIMIT];  /* FILE filename */
    off_t offset;           /* last read offset (info) */
    off_t buffer_offset;    /* current buffer data start */
    uint8_t * buffer;       /* data buffer */
    size_t buffersize;      /* max buffer size */
    size_t validsize;       /* current buffer size */
    size_t filesize;        /* buffered file size */
} STDIO_STREAMFILE;

static STREAMFILE* open_stdio_streamfile_buffer(const char * const filename, size_t buffersize);
static STREAMFILE* open_stdio_streamfile_buffer_by_file(FILE *infile, const char * const filename, size_t buffersize);

static size_t read_stdio(STDIO_STREAMFILE *streamfile, uint8_t *dst, off_t offset, size_t length) {
    size_t length_read_total = 0;

    if (!streamfile->infile || !dst || length <= 0 || offset < 0)
        return 0;

    //;VGM_LOG("STDIO: read %lx + %x (buf %lx + %x)\n", offset, length, streamfile->buffer_offset, streamfile->validsize);

    /* is the part of the requested length in the buffer? */
    if (offset >= streamfile->buffer_offset && offset < streamfile->buffer_offset + streamfile->validsize) {
        size_t length_to_read;
        off_t offset_into_buffer = offset - streamfile->buffer_offset;

        length_to_read = streamfile->validsize - offset_into_buffer;
        if (length_to_read > length)
            length_to_read = length;

        //;VGM_LOG("STDIO: copy buf %lx + %x (+ %x) (buf %lx + %x)\n", offset, length_to_read, (length - length_to_read), streamfile->buffer_offset, streamfile->validsize);

        memcpy(dst, streamfile->buffer + offset_into_buffer, length_to_read);
        length_read_total += length_to_read;
        length -= length_to_read;
        offset += length_to_read;
        dst += length_to_read;
    }

#ifdef VGM_DEBUG_OUTPUT
    if (offset < streamfile->buffer_offset && length > 0) {
        VGM_LOG("STDIO: rebuffer, requested %lx vs %lx (sf %x)\n", offset, streamfile->buffer_offset, (uint32_t)streamfile);
        //streamfile->rebuffer++;
        //if (rebuffer > N) ...
    }
#endif

    /* read the rest of the requested length */
    while (length > 0) {
        size_t length_to_read;

        /* ignore requests at EOF */
        if (offset >= streamfile->filesize) {
            //offset = streamfile->filesize; /* seems fseek doesn't clamp offset */
            VGM_ASSERT_ONCE(offset > streamfile->filesize, "STDIO: reading over filesize 0x%x @ 0x%x + 0x%x\n", streamfile->filesize, (uint32_t)offset, length);
            break;
        }

        /* position to new offset */
        if (fseeko(streamfile->infile,offset,SEEK_SET)) {
            break; /* this shouldn't happen in our code */
        }

#ifdef _MSC_VER
        /* Workaround a bug that appears when compiling with MSVC (later versions).
         * This bug is deterministic and seemingly appears randomly after seeking.
         * It results in fread returning data from the wrong area of the file.
         * HPS is one format that is almost always affected by this.
         * May be related/same as open_stdio's bug when using dup() */
        fseek(streamfile->infile, ftell(streamfile->infile), SEEK_SET);
#endif

        /* fill the buffer (offset now is beyond buffer_offset) */
        streamfile->buffer_offset = offset;
        streamfile->validsize = fread(streamfile->buffer, sizeof(uint8_t), streamfile->buffersize, streamfile->infile);
        //;VGM_LOG("STDIO: read buf %lx + %x\n", streamfile->buffer_offset, streamfile->validsize);

        /* decide how much must be read this time */
        if (length > streamfile->buffersize)
            length_to_read = streamfile->buffersize;
        else
            length_to_read = length;

        /* give up on partial reads (EOF) */
        if (streamfile->validsize < length_to_read) {
            memcpy(dst, streamfile->buffer, streamfile->validsize);
            offset += streamfile->validsize;
            length_read_total += streamfile->validsize;
            break;
        }

        /* use the new buffer */
        memcpy(dst, streamfile->buffer, length_to_read);
        offset += length_to_read;
        length_read_total += length_to_read;
        length -= length_to_read;
        dst += length_to_read;
    }

    streamfile->offset = offset; /* last fread offset */
    return length_read_total;
}
static size_t get_size_stdio(STDIO_STREAMFILE *streamfile) {
    return streamfile->filesize;
}
static off_t get_offset_stdio(STDIO_STREAMFILE *streamfile) {
    return streamfile->offset;
}
static void get_name_stdio(STDIO_STREAMFILE *streamfile, char *buffer, size_t length) {
    strncpy(buffer, streamfile->name, length);
    buffer[length-1]='\0';
}
static void close_stdio(STDIO_STREAMFILE *streamfile) {
    if (streamfile->infile)
        fclose(streamfile->infile);
    free(streamfile->buffer);
    free(streamfile);
}

static STREAMFILE* open_stdio(STDIO_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    if (!filename)
        return NULL;

#if !defined (__ANDROID__) && !defined (_MSC_VER)
    /* when enabling this for MSVC it'll seemingly work, but there are issues possibly related to underlying
     * IO buffers when using dup(), noticeable by re-opening the same streamfile with small buffer sizes
     * (reads garbage). fseek bug in line 81 may be related/same thing and may be removed.
     * this reportedly this causes issues in Android too */

    /* if same name, duplicate the file descriptor we already have open */
    if (streamfile->infile && !strcmp(streamfile->name,filename)) {
        int new_fd;
        FILE *new_file = NULL;

        if (((new_fd = dup(fileno(streamfile->infile))) >= 0) && (new_file = fdopen(new_fd, "rb")))  {
            STREAMFILE *new_sf = open_stdio_streamfile_buffer_by_file(new_file, filename, buffersize);
            if (new_sf)
                return new_sf;
            fclose(new_file);
        }
        if (new_fd >= 0 && !new_file)
            close(new_fd); /* fdopen may fail when opening too many files */

        /* on failure just close and try the default path (which will probably fail a second time) */
    }
#endif    
    // a normal open, open a new file
    return open_stdio_streamfile_buffer(filename, buffersize);
}

static STREAMFILE* open_stdio_streamfile_buffer_by_file(FILE *infile, const char * const filename, size_t buffersize) {
    uint8_t *buffer = NULL;
    STDIO_STREAMFILE *streamfile = NULL;

    buffer = calloc(buffersize,1);
    if (!buffer) goto fail;

    streamfile = calloc(1,sizeof(STDIO_STREAMFILE));
    if (!streamfile) goto fail;

    streamfile->sf.read = (void*)read_stdio;
    streamfile->sf.get_size = (void*)get_size_stdio;
    streamfile->sf.get_offset = (void*)get_offset_stdio;
    streamfile->sf.get_name = (void*)get_name_stdio;
    streamfile->sf.open = (void*)open_stdio;
    streamfile->sf.close = (void*)close_stdio;

    streamfile->infile = infile;
    streamfile->buffersize = buffersize;
    streamfile->buffer = buffer;

    strncpy(streamfile->name, filename, sizeof(streamfile->name));
    streamfile->name[sizeof(streamfile->name)-1] = '\0';

    /* cache filesize */
    if (infile) {
        fseeko(streamfile->infile,0,SEEK_END);
        streamfile->filesize = ftello(streamfile->infile);
    }
    else {
        streamfile->filesize = 0; /* allow virtual, non-existing files */
    }

    /* Typically fseek(o)/ftell(o) may only handle up to ~2.14GB, signed 32b = 0x7FFFFFFF
     * (happens in banks like FSB, though rarely). Can be remedied with the
     * preprocessor (-D_FILE_OFFSET_BITS=64 in GCC) but it's not well tested. */
    if (streamfile->filesize == 0xFFFFFFFF) { /* -1 on error */
        VGM_LOG("STREAMFILE: ftell error\n");
        goto fail; /* can be ignored but may result in strange/unexpected behaviors */
    }

    return &streamfile->sf;

fail:
    free(buffer);
    free(streamfile);
    return NULL;
}

static STREAMFILE* open_stdio_streamfile_buffer(const char * const filename, size_t bufsize) {
    FILE *infile = NULL;
    STREAMFILE *streamfile = NULL;

    infile = fopen(filename,"rb");
    if (!infile) {
        /* allow non-existing files in some cases */
        if (!vgmstream_is_virtual_filename(filename))
            return NULL;
    }

    streamfile = open_stdio_streamfile_buffer_by_file(infile, filename, bufsize);
    if (!streamfile) {
        if (infile) fclose(infile);
    }

    return streamfile;
}

STREAMFILE* open_stdio_streamfile(const char *filename) {
    return open_stdio_streamfile_buffer(filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE* open_stdio_streamfile_by_file(FILE *file, const char *filename) {
    return open_stdio_streamfile_buffer_by_file(file, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
    off_t offset;           /* last read offset (info) */
    off_t buffer_offset;    /* current buffer data start */
    uint8_t * buffer;       /* data buffer */
    size_t buffersize;      /* max buffer size */
    size_t validsize;       /* current buffer size */
    size_t filesize;        /* buffered file size */
} BUFFER_STREAMFILE;


static size_t buffer_read(BUFFER_STREAMFILE *streamfile, uint8_t *dst, off_t offset, size_t length) {
    size_t length_read_total = 0;

    if (!dst || length <= 0 || offset < 0)
        return 0;

    /* is the part of the requested length in the buffer? */
    if (offset >= streamfile->buffer_offset && offset < streamfile->buffer_offset + streamfile->validsize) {
        size_t length_to_read;
        off_t offset_into_buffer = offset - streamfile->buffer_offset;

        length_to_read = streamfile->validsize - offset_into_buffer;
        if (length_to_read > length)
            length_to_read = length;

        memcpy(dst, streamfile->buffer + offset_into_buffer, length_to_read);
        length_read_total += length_to_read;
        length -= length_to_read;
        offset += length_to_read;
        dst += length_to_read;
    }

#ifdef VGM_DEBUG_OUTPUT
    if (offset < streamfile->buffer_offset) {
        VGM_LOG("BUFFER: rebuffer, requested %lx vs %lx (sf %x)\n", offset, streamfile->buffer_offset, (uint32_t)streamfile);
    }
#endif

    /* read the rest of the requested length */
    while (length > 0) {
        size_t length_to_read;

        /* ignore requests at EOF */
        if (offset >= streamfile->filesize) {
            //offset = streamfile->filesize; /* seems fseek doesn't clamp offset */
            VGM_ASSERT_ONCE(offset > streamfile->filesize, "BUFFER: reading over filesize 0x%x @ 0x%x + 0x%x\n", streamfile->filesize, (uint32_t)offset, length);
            break;
        }

        /* fill the buffer (offset now is beyond buffer_offset) */
        streamfile->buffer_offset = offset;
        streamfile->validsize = streamfile->inner_sf->read(streamfile->inner_sf, streamfile->buffer, streamfile->buffer_offset, streamfile->buffersize);

        /* decide how much must be read this time */
        if (length > streamfile->buffersize)
            length_to_read = streamfile->buffersize;
        else
            length_to_read = length;

        /* give up on partial reads (EOF) */
        if (streamfile->validsize < length_to_read) {
            memcpy(dst, streamfile->buffer, streamfile->validsize);
            offset += streamfile->validsize;
            length_read_total += streamfile->validsize;
            break;
        }

        /* use the new buffer */
        memcpy(dst, streamfile->buffer, length_to_read);
        offset += length_to_read;
        length_read_total += length_to_read;
        length -= length_to_read;
        dst += length_to_read;
    }

    streamfile->offset = offset; /* last fread offset */
    return length_read_total;
}
static size_t buffer_get_size(BUFFER_STREAMFILE *streamfile) {
    return streamfile->filesize; /* cache */
}
static size_t buffer_get_offset(BUFFER_STREAMFILE *streamfile) {
    return streamfile->offset; /* cache */
}
static void buffer_get_name(BUFFER_STREAMFILE *streamfile, char *buffer, size_t length) {
    streamfile->inner_sf->get_name(streamfile->inner_sf, buffer, length); /* default */
}
static STREAMFILE *buffer_open(BUFFER_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    STREAMFILE *new_inner_sf = streamfile->inner_sf->open(streamfile->inner_sf,filename,buffersize);
    return open_buffer_streamfile(new_inner_sf, buffersize); /* original buffer size is preferable? */
}
static void buffer_close(BUFFER_STREAMFILE *streamfile) {
    streamfile->inner_sf->close(streamfile->inner_sf);
    free(streamfile->buffer);
    free(streamfile);
}

STREAMFILE* open_buffer_streamfile(STREAMFILE *streamfile, size_t buffer_size) {
    BUFFER_STREAMFILE *this_sf = NULL;

    if (!streamfile) goto fail;

    this_sf = calloc(1, sizeof(BUFFER_STREAMFILE));
    if (!this_sf) goto fail;

    this_sf->buffersize = buffer_size;
    if (this_sf->buffersize == 0)
        this_sf->buffersize = STREAMFILE_DEFAULT_BUFFER_SIZE;

    this_sf->buffer = calloc(this_sf->buffersize,1);
    if (!this_sf->buffer) goto fail;

    /* set callbacks and internals */
    this_sf->sf.read = (void*)buffer_read;
    this_sf->sf.get_size = (void*)buffer_get_size;
    this_sf->sf.get_offset = (void*)buffer_get_offset;
    this_sf->sf.get_name = (void*)buffer_get_name;
    this_sf->sf.open = (void*)buffer_open;
    this_sf->sf.close = (void*)buffer_close;
    this_sf->sf.stream_index = streamfile->stream_index;

    this_sf->inner_sf = streamfile;

    this_sf->filesize = streamfile->get_size(streamfile);

    return &this_sf->sf;

fail:
    if (this_sf) free(this_sf->buffer);
    free(this_sf);
    return NULL;
}
STREAMFILE* open_buffer_streamfile_f(STREAMFILE *streamfile, size_t buffer_size) {
    STREAMFILE *new_sf = open_buffer_streamfile(streamfile, buffer_size);
    if (!new_sf)
        close_streamfile(streamfile);
    return new_sf;
}

/* **************************************************** */

//todo stream_index: copy? pass? funtion? external?
//todo use realnames on reopen? simplify?
//todo use safe string ops, this ain't easy

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
} WRAP_STREAMFILE;

static size_t wrap_read(WRAP_STREAMFILE *streamfile, uint8_t *dst, off_t offset, size_t length) {
    return streamfile->inner_sf->read(streamfile->inner_sf, dst, offset, length); /* default */
}
static size_t wrap_get_size(WRAP_STREAMFILE *streamfile) {
    return streamfile->inner_sf->get_size(streamfile->inner_sf); /* default */
}
static size_t wrap_get_offset(WRAP_STREAMFILE *streamfile) {
    return streamfile->inner_sf->get_offset(streamfile->inner_sf); /* default */
}
static void wrap_get_name(WRAP_STREAMFILE *streamfile, char *buffer, size_t length) {
    streamfile->inner_sf->get_name(streamfile->inner_sf, buffer, length); /* default */
}
static void wrap_open(WRAP_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    streamfile->inner_sf->open(streamfile->inner_sf, filename, buffersize); /* default (don't wrap) */
}
static void wrap_close(WRAP_STREAMFILE *streamfile) {
    //streamfile->inner_sf->close(streamfile->inner_sf); /* don't close */
    free(streamfile);
}

STREAMFILE* open_wrap_streamfile(STREAMFILE *streamfile) {
    WRAP_STREAMFILE *this_sf = NULL;

    if (!streamfile) return NULL;

    this_sf = calloc(1,sizeof(WRAP_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->sf.read = (void*)wrap_read;
    this_sf->sf.get_size = (void*)wrap_get_size;
    this_sf->sf.get_offset = (void*)wrap_get_offset;
    this_sf->sf.get_name = (void*)wrap_get_name;
    this_sf->sf.open = (void*)wrap_open;
    this_sf->sf.close = (void*)wrap_close;
    this_sf->sf.stream_index = streamfile->stream_index;

    this_sf->inner_sf = streamfile;

    return &this_sf->sf;
}
STREAMFILE* open_wrap_streamfile_f(STREAMFILE* streamfile) {
    STREAMFILE* new_sf = open_wrap_streamfile(streamfile);
    if (!new_sf)
        close_streamfile(streamfile);
    return new_sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE* inner_sf;
    off_t start;
    size_t size;
} CLAMP_STREAMFILE;

static size_t clamp_read(CLAMP_STREAMFILE* streamfile, uint8_t* dst, off_t offset, size_t length) {
    off_t inner_offset = streamfile->start + offset;
    size_t clamp_length = length;

    if (offset + length > streamfile->size) {
        if (offset >= streamfile->size)
            clamp_length = 0;
        else
            clamp_length = streamfile->size - offset;
    }

    return streamfile->inner_sf->read(streamfile->inner_sf, dst, inner_offset, clamp_length);
}
static size_t clamp_get_size(CLAMP_STREAMFILE* streamfile) {
    return streamfile->size;
}
static off_t clamp_get_offset(CLAMP_STREAMFILE* streamfile) {
    return streamfile->inner_sf->get_offset(streamfile->inner_sf) - streamfile->start;
}
static void clamp_get_name(CLAMP_STREAMFILE* streamfile, char* buffer, size_t length) {
    streamfile->inner_sf->get_name(streamfile->inner_sf, buffer, length); /* default */
}
static STREAMFILE* clamp_open(CLAMP_STREAMFILE* streamfile, const char* const filename, size_t buffersize) {
    char original_filename[PATH_LIMIT];
    STREAMFILE* new_inner_sf = NULL;

    new_inner_sf = streamfile->inner_sf->open(streamfile->inner_sf,filename,buffersize);
    streamfile->inner_sf->get_name(streamfile->inner_sf, original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) {
        return open_clamp_streamfile(new_inner_sf, streamfile->start, streamfile->size); /* clamp again */
    } else {
        return new_inner_sf;
    }
}
static void clamp_close(CLAMP_STREAMFILE* streamfile) {
    streamfile->inner_sf->close(streamfile->inner_sf);
    free(streamfile);
}

STREAMFILE* open_clamp_streamfile(STREAMFILE* streamfile, off_t start, size_t size) {
    CLAMP_STREAMFILE* this_sf = NULL;

    if (!streamfile || size == 0) return NULL;
    if (start + size > get_streamfile_size(streamfile)) return NULL;

    this_sf = calloc(1,sizeof(CLAMP_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->sf.read = (void*)clamp_read;
    this_sf->sf.get_size = (void*)clamp_get_size;
    this_sf->sf.get_offset = (void*)clamp_get_offset;
    this_sf->sf.get_name = (void*)clamp_get_name;
    this_sf->sf.open = (void*)clamp_open;
    this_sf->sf.close = (void*)clamp_close;
    this_sf->sf.stream_index = streamfile->stream_index;

    this_sf->inner_sf = streamfile;
    this_sf->start = start;
    this_sf->size = size;

    return &this_sf->sf;
}
STREAMFILE* open_clamp_streamfile_f(STREAMFILE* streamfile, off_t start, size_t size) {
    STREAMFILE* new_sf = open_clamp_streamfile(streamfile, start, size);
    if (!new_sf)
        close_streamfile(streamfile);
    return new_sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
    void* data; /* state for custom reads, malloc'ed + copied on open (to re-open streamfiles cleanly) */
    size_t data_size;
    size_t (*read_callback)(STREAMFILE *, uint8_t *, off_t, size_t, void*); /* custom read to modify data before copying into buffer */
    size_t (*size_callback)(STREAMFILE *, void*); /* size when custom reads make data smaller/bigger than underlying streamfile */
    int (*init_callback)(STREAMFILE*, void*); /* init the data struct members somehow, return >= 0 if ok */
    void (*close_callback)(STREAMFILE*, void*); /* close the data struct members somehow */
} IO_STREAMFILE;

static size_t io_read(IO_STREAMFILE *streamfile, uint8_t *dst, off_t offset, size_t length) {
    return streamfile->read_callback(streamfile->inner_sf, dst, offset, length, streamfile->data);
}
static size_t io_get_size(IO_STREAMFILE *streamfile) {
    if (streamfile->size_callback)
        return streamfile->size_callback(streamfile->inner_sf, streamfile->data);
    else
        return streamfile->inner_sf->get_size(streamfile->inner_sf); /* default */
}
static off_t io_get_offset(IO_STREAMFILE *streamfile) {
    return streamfile->inner_sf->get_offset(streamfile->inner_sf);  /* default */
}
static void io_get_name(IO_STREAMFILE *streamfile, char *buffer, size_t length) {
    streamfile->inner_sf->get_name(streamfile->inner_sf, buffer, length); /* default */
}
static STREAMFILE* io_open(IO_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    STREAMFILE *new_inner_sf = streamfile->inner_sf->open(streamfile->inner_sf,filename,buffersize);
    return open_io_streamfile_ex(new_inner_sf, streamfile->data, streamfile->data_size, streamfile->read_callback, streamfile->size_callback, streamfile->init_callback, streamfile->close_callback);
}
static void io_close(IO_STREAMFILE *streamfile) {
    if (streamfile->close_callback)
        streamfile->close_callback(streamfile->inner_sf, streamfile->data);
    streamfile->inner_sf->close(streamfile->inner_sf);
    free(streamfile->data);
    free(streamfile);
}

STREAMFILE* open_io_streamfile_ex(STREAMFILE *streamfile, void *data, size_t data_size, void *read_callback, void *size_callback, void* init_callback, void* close_callback) {
    IO_STREAMFILE *this_sf = NULL;

    if (!streamfile) goto fail;
    if ((data && !data_size) || (!data && data_size)) goto fail;

    this_sf = calloc(1,sizeof(IO_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->sf.read = (void*)io_read;
    this_sf->sf.get_size = (void*)io_get_size;
    this_sf->sf.get_offset = (void*)io_get_offset;
    this_sf->sf.get_name = (void*)io_get_name;
    this_sf->sf.open = (void*)io_open;
    this_sf->sf.close = (void*)io_close;
    this_sf->sf.stream_index = streamfile->stream_index;

    this_sf->inner_sf = streamfile;
    if (data) {
        this_sf->data = malloc(data_size);
        if (!this_sf->data) goto fail;
        memcpy(this_sf->data, data, data_size);
    }
    this_sf->data_size = data_size;
    this_sf->read_callback = read_callback;
    this_sf->size_callback = size_callback;
    this_sf->init_callback = init_callback;
    this_sf->close_callback = close_callback;

    if (this_sf->init_callback) {
        int ok = this_sf->init_callback(this_sf->inner_sf, this_sf->data);
        if (ok < 0) goto fail;
    }

    return &this_sf->sf;
    
fail:
    if (this_sf) free(this_sf->data);
    free(this_sf);
    return NULL;
}

STREAMFILE* open_io_streamfile_ex_f(STREAMFILE *streamfile, void *data, size_t data_size, void *read_callback, void *size_callback, void* init_callback, void* close_callback) {
    STREAMFILE *new_sf = open_io_streamfile_ex(streamfile, data, data_size, read_callback, size_callback, init_callback, close_callback);
    if (!new_sf)
        close_streamfile(streamfile);
    return new_sf;
}

STREAMFILE* open_io_streamfile(STREAMFILE *streamfile, void *data, size_t data_size, void *read_callback, void *size_callback) {
    return open_io_streamfile_ex(streamfile, data, data_size, read_callback, size_callback, NULL, NULL);
}
STREAMFILE* open_io_streamfile_f(STREAMFILE *streamfile, void *data, size_t data_size, void *read_callback, void *size_callback) {
    return open_io_streamfile_ex_f(streamfile, data, data_size, read_callback, size_callback, NULL, NULL);
}

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
    char fakename[PATH_LIMIT];
} FAKENAME_STREAMFILE;

static size_t fakename_read(FAKENAME_STREAMFILE *streamfile, uint8_t *dst, off_t offset, size_t length) {
    return streamfile->inner_sf->read(streamfile->inner_sf, dst, offset, length); /* default */
}
static size_t fakename_get_size(FAKENAME_STREAMFILE *streamfile) {
    return streamfile->inner_sf->get_size(streamfile->inner_sf); /* default */
}
static size_t fakename_get_offset(FAKENAME_STREAMFILE *streamfile) {
    return streamfile->inner_sf->get_offset(streamfile->inner_sf); /* default */
}
static void fakename_get_name(FAKENAME_STREAMFILE *streamfile, char *buffer, size_t length) {
    strncpy(buffer,streamfile->fakename,length);
    buffer[length-1]='\0';
}
static STREAMFILE* fakename_open(FAKENAME_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    /* detect re-opening the file */
    if (strcmp(filename, streamfile->fakename) == 0) {
        STREAMFILE *new_inner_sf;
        char original_filename[PATH_LIMIT];

        streamfile->inner_sf->get_name(streamfile->inner_sf, original_filename, PATH_LIMIT);
        new_inner_sf = streamfile->inner_sf->open(streamfile->inner_sf, original_filename, buffersize);
        return open_fakename_streamfile(new_inner_sf, streamfile->fakename, NULL);
    }
    else {
        return streamfile->inner_sf->open(streamfile->inner_sf, filename, buffersize);
    }
}
static void fakename_close(FAKENAME_STREAMFILE *streamfile) {
    streamfile->inner_sf->close(streamfile->inner_sf);
    free(streamfile);
}

STREAMFILE* open_fakename_streamfile(STREAMFILE *streamfile, const char *fakename, const char *fakeext) {
    FAKENAME_STREAMFILE *this_sf = NULL;

    if (!streamfile || (!fakename && !fakeext)) return NULL;

    this_sf = calloc(1,sizeof(FAKENAME_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->sf.read = (void*)fakename_read;
    this_sf->sf.get_size = (void*)fakename_get_size;
    this_sf->sf.get_offset = (void*)fakename_get_offset;
    this_sf->sf.get_name = (void*)fakename_get_name;
    this_sf->sf.open = (void*)fakename_open;
    this_sf->sf.close = (void*)fakename_close;
    this_sf->sf.stream_index = streamfile->stream_index;

    this_sf->inner_sf = streamfile;

    /* copy passed name or retain current, and swap extension if expected */
    if (fakename) {
        strcpy(this_sf->fakename,fakename);
    } else {
        streamfile->get_name(streamfile, this_sf->fakename, PATH_LIMIT);
    }

    if (fakeext) {
        char* ext = strrchr(this_sf->fakename,'.');
        if (ext != NULL) {
            ext[1] = '\0'; /* truncate past dot */
        } else {
            strcat(this_sf->fakename, "."); /* no extension = add dot */
        }
        strcat(this_sf->fakename, fakeext);
    }

    return &this_sf->sf;
}
STREAMFILE* open_fakename_streamfile_f(STREAMFILE *streamfile, const char *fakename, const char *fakeext) {
    STREAMFILE *new_sf = open_fakename_streamfile(streamfile, fakename, fakeext);
    if (!new_sf)
        close_streamfile(streamfile);
    return new_sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE **inner_sfs;
    size_t inner_sfs_size;
    size_t *sizes;
    off_t size;
    off_t offset;
} MULTIFILE_STREAMFILE;

static size_t multifile_read(MULTIFILE_STREAMFILE *streamfile, uint8_t *dst, off_t offset, size_t length) {
    int i, segment = 0;
    off_t segment_offset = 0;
    size_t done = 0;

    if (offset > streamfile->size) {
        streamfile->offset = streamfile->size;
        return 0;
    }

    /* map external offset to multifile offset */
    for (i = 0; i < streamfile->inner_sfs_size; i++) {
        size_t segment_size = streamfile->sizes[i];
        /* check if offset falls in this segment */
        if (offset >= segment_offset && offset < segment_offset + segment_size) {
            segment = i;
            segment_offset = offset - segment_offset;
            break;
        }

        segment_offset += segment_size;
    }

    /* reads can span multiple segments */
    while(done < length) {
        if (segment >= streamfile->inner_sfs_size) /* over last segment, not fully done */
            break;
        /* reads over segment size are ok, will return smaller value and continue next segment */
        done += streamfile->inner_sfs[segment]->read(streamfile->inner_sfs[segment], dst + done, segment_offset, length - done);
        segment++;
        segment_offset = 0;
    }

    streamfile->offset = offset + done;
    return done;
}
static size_t multifile_get_size(MULTIFILE_STREAMFILE *streamfile) {
    return streamfile->size;
}
static size_t multifile_get_offset(MULTIFILE_STREAMFILE * streamfile) {
    return streamfile->offset;
}
static void multifile_get_name(MULTIFILE_STREAMFILE *streamfile, char *buffer, size_t length) {
    streamfile->inner_sfs[0]->get_name(streamfile->inner_sfs[0], buffer, length);
}
static STREAMFILE* multifile_open(MULTIFILE_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    char original_filename[PATH_LIMIT];
    STREAMFILE *new_sf = NULL;
    STREAMFILE **new_inner_sfs = NULL;
    int i;

    streamfile->inner_sfs[0]->get_name(streamfile->inner_sfs[0], original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) { /* same multifile */
        new_inner_sfs = calloc(streamfile->inner_sfs_size, sizeof(STREAMFILE*));
        if (!new_inner_sfs) goto fail;

        for (i = 0; i < streamfile->inner_sfs_size; i++) {
            streamfile->inner_sfs[i]->get_name(streamfile->inner_sfs[i], original_filename, PATH_LIMIT);
            new_inner_sfs[i] = streamfile->inner_sfs[i]->open(streamfile->inner_sfs[i], original_filename, buffersize);
            if (!new_inner_sfs[i]) goto fail;
        }

        new_sf = open_multifile_streamfile(new_inner_sfs, streamfile->inner_sfs_size);
        if (!new_sf) goto fail;

        free(new_inner_sfs);
        return new_sf;
    }
    else {
        return streamfile->inner_sfs[0]->open(streamfile->inner_sfs[0], filename, buffersize); /* regular file */
    }

fail:
    if (new_inner_sfs) {
        for (i = 0; i < streamfile->inner_sfs_size; i++)
            close_streamfile(new_inner_sfs[i]);
    }
    free(new_inner_sfs);
    return NULL;
}
static void multifile_close(MULTIFILE_STREAMFILE *streamfile) {
    int i;
    for (i = 0; i < streamfile->inner_sfs_size; i++) {
        for (i = 0; i < streamfile->inner_sfs_size; i++) {
            close_streamfile(streamfile->inner_sfs[i]);
        }
    }
    free(streamfile->inner_sfs);
    free(streamfile->sizes);
    free(streamfile);
}

STREAMFILE* open_multifile_streamfile(STREAMFILE **streamfiles, size_t streamfiles_size) {
    MULTIFILE_STREAMFILE *this_sf = NULL;
    int i;

    if (!streamfiles || !streamfiles_size) return NULL;

    for (i = 0; i < streamfiles_size; i++) {
        if (!streamfiles[i]) return NULL;
    }

    this_sf = calloc(1,sizeof(MULTIFILE_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->sf.read = (void*)multifile_read;
    this_sf->sf.get_size = (void*)multifile_get_size;
    this_sf->sf.get_offset = (void*)multifile_get_offset;
    this_sf->sf.get_name = (void*)multifile_get_name;
    this_sf->sf.open = (void*)multifile_open;
    this_sf->sf.close = (void*)multifile_close;
    this_sf->sf.stream_index = streamfiles[0]->stream_index;

    this_sf->inner_sfs_size = streamfiles_size;
    this_sf->inner_sfs = calloc(streamfiles_size, sizeof(STREAMFILE*));
    if (!this_sf->inner_sfs) goto fail;
    this_sf->sizes = calloc(streamfiles_size, sizeof(size_t));
    if (!this_sf->sizes) goto fail;

    for (i = 0; i < this_sf->inner_sfs_size; i++) {
        this_sf->inner_sfs[i] = streamfiles[i];
        this_sf->sizes[i] = streamfiles[i]->get_size(streamfiles[i]);
        this_sf->size += this_sf->sizes[i];
    }

    return &this_sf->sf;

fail:
    if (this_sf) {
        free(this_sf->inner_sfs);
        free(this_sf->sizes);
    }
    free(this_sf);
    return NULL;
}
STREAMFILE* open_multifile_streamfile_f(STREAMFILE **streamfiles, size_t streamfiles_size) {
    STREAMFILE *new_sf = open_multifile_streamfile(streamfiles, streamfiles_size);
    if (!new_sf) {
        int i;
        for (i = 0; i < streamfiles_size; i++) {
            close_streamfile(streamfiles[i]);
        }
    }
    return new_sf;
}

/* **************************************************** */

STREAMFILE* open_streamfile(STREAMFILE* sf, const char* pathname) {
    return sf->open(sf, pathname, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE* open_streamfile_by_ext(STREAMFILE* sf, const char* ext) {
    char filename[PATH_LIMIT];
    int filename_len, fileext_len;

    sf->get_name(sf, filename, sizeof(filename));

    filename_len = strlen(filename);
    fileext_len = strlen(filename_extension(filename));

    if (fileext_len == 0) {/* extensionless */
        strcat(filename,".");
        strcat(filename,ext);
    }
    else {
        strcpy(filename + filename_len - fileext_len, ext);
    }

    return sf->open(sf, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE* open_streamfile_by_filename(STREAMFILE* sf, const char* filename) {
    char fullname[PATH_LIMIT];
    char partname[PATH_LIMIT];
    char *path, *name, *otherpath;

    if (!sf || !filename || !filename[0]) return NULL;

    sf->get_name(sf, fullname, sizeof(fullname));

    //todo normalize separators in a better way, safeops, improve copying

    /* check for non-normalized paths first (ex. txth) */
    path = strrchr(fullname, '/');
    otherpath = strrchr(fullname, '\\');
    if (otherpath > path) { //todo cast to ptr?
        /* foobar makes paths like "(fake protocol)://(windows path with \)".
         * Hack to work around both separators, though probably foo_streamfile
         * should just return and handle normalized paths without protocol. */
        path = otherpath;
    }

    if (path) {
        path[1] = '\0'; /* remove name after separator */

        strcpy(partname, filename);
        fix_dir_separators(partname); /* normalize to DIR_SEPARATOR */

        /* normalize relative paths as don't work ok in some plugins */
        if (partname[0] == '.' && partname[1] == DIR_SEPARATOR) { /* './name' */
            name = partname + 2; /* ignore './' */
        }
        else if (partname[0] == '.' && partname[1] == '.' && partname[2] == DIR_SEPARATOR) { /* '../name' */
            char *pathprev;

            path[0] = '\0'; /* remove last separator so next call works */
            pathprev = strrchr(fullname,DIR_SEPARATOR);
            if (pathprev) {
                pathprev[1] = '\0'; /* remove prev dir after separator */
                name = partname + 3; /* ignore '../' */
            }
            else { /* let plugin handle? */
                path[0] = DIR_SEPARATOR;
                name = partname;
            }
            /* could work with more relative paths but whatevs */
        }
        else {
            name = partname;
        }

        strcat(fullname, name);
    }
    else {
        strcpy(fullname, filename);
    }

    return sf->open(sf, fullname, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE* reopen_streamfile(STREAMFILE* sf, size_t buffer_size) {
    char pathname[PATH_LIMIT];

    if (!sf) return NULL;

    if (buffer_size == 0)
        buffer_size = STREAMFILE_DEFAULT_BUFFER_SIZE;
    sf->get_name(sf, pathname,sizeof(pathname));
    return sf->open(sf, pathname, buffer_size);
}

/* **************************************************** */

size_t read_line(char* buf, int buf_size, off_t offset, STREAMFILE* sf, int* p_line_ok) {
    int i;
    off_t file_size = get_streamfile_size(sf);
    int extra_bytes = 0; /* how many bytes over those put in the buffer were read */

    if (p_line_ok) *p_line_ok = 0;

    for (i = 0; i < buf_size-1 && offset+i < file_size; i++) {
        char in_char = read_8bit(offset+i, sf);
        /* check for end of line */
        if (in_char == 0x0d && read_8bit(offset+i+1, sf) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            if (p_line_ok) *p_line_ok = 1;
            break;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            if (p_line_ok) *p_line_ok = 1;
            break;
        }

        buf[i] = in_char;
    }

    buf[i] = '\0';

    /* did we fill the buffer? */
    if (i == buf_size) {
        char in_char = read_8bit(offset+i, sf);
        /* did the bytes we missed just happen to be the end of the line? */
        if (in_char == 0x0d && read_8bit(offset+i+1, sf) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            if (p_line_ok) *p_line_ok = 1;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            if (p_line_ok) *p_line_ok = 1;
        }
    }

    /* did we hit the file end? */
    if (offset+i == file_size) {
        /* then we did in fact finish reading the last line */
        if (p_line_ok) *p_line_ok = 1;
    }

    return i + extra_bytes;
}

size_t read_bom(STREAMFILE* sf) {
    if (read_u16le(0x00, sf) == 0xFFFE ||
        read_u16le(0x00, sf) == 0xFEFF) {
        return 0x02;
    }

    if ((read_u32be(0x00, sf) & 0xFFFFFF00) == 0xEFBBBF00) {
        return 0x03;
    }

    return 0x00;
}

size_t read_string(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    size_t pos;

    for (pos = 0; pos < buf_size; pos++) {
        uint8_t byte = read_u8(offset + pos, sf);
        char c = (char)byte;
        if (buf) buf[pos] = c;
        if (c == '\0')
            return pos;
        if (pos+1 == buf_size) { /* null at maxsize and don't validate (expected to be garbage) */
            if (buf) buf[pos] = '\0';
            return buf_size;
        }
        /* UTF-8 only goes to 0x7F, but allow a bunch of Windows-1252 codes that some games use */
        if (byte < 0x20 || byte > 0xF0)
            goto fail;
    }

fail:
    if (buf) buf[0] = '\0';
    return 0;
}

size_t read_string_utf16(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf, int big_endian) {
    size_t pos, offpos;
    uint16_t (*read_u16)(off_t,STREAMFILE*) = big_endian ? read_u16be : read_u16le;


    for (pos = 0, offpos = 0; pos < buf_size; pos++, offpos += 2) {
        char c = read_u16(offset + offpos, sf) & 0xFF; /* lower byte for now */
        if (buf) buf[pos] = c;
        if (c == '\0')
            return pos;
        if (pos+1 == buf_size) { /* null at maxsize and don't validate (expected to be garbage) */
            if (buf) buf[pos] = '\0';
            return buf_size;
        }
        if (c < 0x20 || (uint8_t)c > 0xA5)
            goto fail;
    }

fail:
    if (buf) buf[0] = '\0';
    return 0;
}

size_t read_string_utf16le(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    return read_string_utf16(buf, buf_size, offset, sf, 0);
}
size_t read_string_utf16be(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    return read_string_utf16(buf, buf_size, offset, sf, 1);
}

/* ************************************************************************* */

size_t read_key_file(uint8_t* buf, size_t buf_size, STREAMFILE* sf) {
    char keyname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    const char *path, *ext;
    STREAMFILE* sf_key = NULL;
    size_t keysize;

    get_streamfile_name(sf, filename, sizeof(filename));

    if (strlen(filename)+4 > sizeof(keyname)) goto fail;

    /* try to open a keyfile using variations */
    {
        ext = strrchr(filename,'.');
        if (ext!=NULL) ext = ext+1;

        path = strrchr(filename, DIR_SEPARATOR);
        if (path!=NULL) path = path+1;

        /* "(name.ext)key" */
        strcpy(keyname, filename);
        strcat(keyname, "key");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;

        /* "(name.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;
        */


        /* "(.ext)key" */
        if (path) {
            strcpy(keyname, filename);
            keyname[path-filename] = '\0';
            strcat(keyname, ".");
        } else {
            strcpy(keyname, ".");
        }
        if (ext) strcat(keyname, ext);
        strcat(keyname, "key");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;

        /* "(.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;
        */

        goto fail;
    }

found:
    keysize = get_streamfile_size(sf_key);
    if (keysize > buf_size) goto fail;

    if (read_streamfile(buf, 0, keysize, sf_key) != keysize)
        goto fail;

    close_streamfile(sf_key);
    return keysize;

fail:
    close_streamfile(sf_key);
    return 0;
}

STREAMFILE* read_filemap_file(STREAMFILE* sf, int file_num) {
    return read_filemap_file_pos(sf, file_num, NULL);
}

STREAMFILE* read_filemap_file_pos(STREAMFILE* sf, int file_num, int* p_pos) {
    char filename[PATH_LIMIT];
    off_t txt_offset, file_size;
    STREAMFILE* sf_map = NULL;
    int file_pos = 0;

    sf_map = open_streamfile_by_filename(sf, ".txtm");
    if (!sf_map) goto fail;

    get_streamfile_filename(sf, filename, sizeof(filename));

    txt_offset = read_bom(sf_map);
    file_size = get_streamfile_size(sf_map);

    /* read lines and find target filename, format is (filename): value1, ... valueN */
    while (txt_offset < file_size) {
        char line[0x2000];
        char key[PATH_LIMIT] = { 0 }, val[0x2000] = { 0 };
        int ok, bytes_read, line_ok;

        bytes_read = read_line(line, sizeof(line), txt_offset, sf_map, &line_ok);
        if (!line_ok) goto fail;

        txt_offset += bytes_read;

        /* get key/val (ignores lead/trailing spaces, stops at comment/separator) */
        ok = sscanf(line, " %[^\t#:] : %[^\t#\r\n] ", key, val);
        if (ok != 2) { /* ignore line if no key=val (comment or garbage) */
            continue;  
        }

        if (strcmp(key, filename) == 0) {
            int n;
            char subval[PATH_LIMIT];
            const char* current = val;
            int i;

            for (i = 0; i <= file_num; i++) {
                if (current[0] == '\0')
                    goto fail;

                ok = sscanf(current, " %[^\t#\r\n,]%n ", subval, &n);
                if (ok != 1)
                    goto fail;

                if (i == file_num) {
                    if (p_pos) *p_pos = file_pos;

                    close_streamfile(sf_map);
                    return open_streamfile_by_filename(sf, subval);
                }

                current += n;
                if (current[0] == ',')
                    current++;
            }
        }
        file_pos++;
    }

fail:
    close_streamfile(sf_map);
    return NULL;
}

void fix_dir_separators(char* filename) {
    char c;
    int i = 0;
    while ((c = filename[i]) != '\0') {
        if ((c == '\\' && DIR_SEPARATOR == '/') || (c == '/' && DIR_SEPARATOR == '\\'))
            filename[i] = DIR_SEPARATOR;
        i++;
    }
}

/* ************************************************************************* */

int check_extensions(STREAMFILE* sf, const char* cmp_exts) {
    char filename[PATH_LIMIT];
    const char* ext = NULL;
    const char* cmp_ext = NULL;
    const char* ststr_res = NULL;
    size_t ext_len, cmp_len;

    sf->get_name(sf,filename,sizeof(filename));
    ext = filename_extension(filename);
    ext_len = strlen(ext);

    cmp_ext = cmp_exts;
    do {
        ststr_res = strstr(cmp_ext, ",");
        cmp_len = ststr_res == NULL
                  ? strlen(cmp_ext) /* total length if more not found */
                  : (intptr_t)ststr_res - (intptr_t)cmp_ext; /* find next ext; ststr_res should always be greater than cmp_ext, resulting in a positive cmp_len */

        if (ext_len == cmp_len && strncasecmp(ext,cmp_ext, ext_len) == 0)
            return 1;

        cmp_ext = ststr_res;
        if (cmp_ext != NULL)
            cmp_ext = cmp_ext + 1; /* skip comma */

    } while (cmp_ext != NULL);

    return 0;
}

/* ************************************************************************* */

/**
 * Find a chunk starting from an offset, and save its offset/size (if not NULL), with offset after id/size.
 * Works for chunked headers in the form of "chunk_id chunk_size (data)"xN  (ex. RIFF).
 * The start_offset should be the first actual chunk (not "RIFF" or "WAVE" but "fmt ").
 * "full_chunk_size" signals chunk_size includes 4+4+data.
 *
 * returns 0 on failure
 */
static int find_chunk_internal(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int big_endian_type, int big_endian_size, int zero_size_end) {
    int32_t (*read_32bit_type)(off_t,STREAMFILE*) = big_endian_type ? read_32bitBE : read_32bitLE;
    int32_t (*read_32bit_size)(off_t,STREAMFILE*) = big_endian_size ? read_32bitBE : read_32bitLE;
    off_t offset, max_offset;
    size_t file_size = get_streamfile_size(sf);

    if (max_size == 0)
        max_size = file_size;

    offset = start_offset;
    max_offset = offset + max_size;
    if (max_offset > file_size)
        max_offset = file_size;


    /* read chunks */
    while (offset < max_offset) {
        uint32_t chunk_type = read_32bit_type(offset + 0x00,sf);
        uint32_t chunk_size = read_32bit_size(offset + 0x04,sf);
        //;VGM_LOG("CHUNK: type=%x, size=%x at %lx\n", chunk_type, chunk_size, offset);

        if (chunk_type == 0xFFFFFFFF || chunk_size == 0xFFFFFFFF)
            return 0;

        if (chunk_type == chunk_id) {
            if (out_chunk_offset) *out_chunk_offset = offset + 0x08;
            if (out_chunk_size) *out_chunk_size = chunk_size;
            return 1;
        }

        /* empty chunk with 0 size, seen in some formats (XVAG uses it as end marker, Wwise doesn't) */
        if (chunk_size == 0 && zero_size_end)
            return 0;

        offset += full_chunk_size ? chunk_size : 0x08 + chunk_size;
    }

    return 0;
}
int find_chunk_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 1, 0);
}
int find_chunk_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 0, 0);
}
int find_chunk(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int big_endian_size, int zero_size_end) {
    return find_chunk_internal(sf, chunk_id, start_offset, 0, full_chunk_size, out_chunk_offset, out_chunk_size, 1, big_endian_size, zero_size_end);
}
int find_chunk_riff_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, 1, 0, 0);
}
int find_chunk_riff_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, 1, 1, 0);
}
int find_chunk_riff_ve(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size, int big_endian) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, big_endian, big_endian, 0);
}

/* ************************************************************************* */

/* copies name as-is (may include full path included) */
void get_streamfile_name(STREAMFILE* sf, char* buffer, size_t size) {
    sf->get_name(sf, buffer, size);
}

/* copies the filename without path */
void get_streamfile_filename(STREAMFILE* sf, char* buffer, size_t size) {
    char foldername[PATH_LIMIT];
    const char* path;


    get_streamfile_name(sf, foldername, sizeof(foldername));

    //todo Windows CMD accepts both \\ and /, better way to handle this?
    path = strrchr(foldername,'\\');
    if (!path)
        path = strrchr(foldername,'/');
    if (path != NULL)
        path = path+1;

    //todo validate sizes and copy sensible max
    if (path) {
        strcpy(buffer, path);
    } else {
        strcpy(buffer, foldername);
    }
}

/* copies the filename without path or extension */
void get_streamfile_basename(STREAMFILE* sf, char* buffer, size_t size) {
    char* ext;

    get_streamfile_filename(sf, buffer, size);

    ext = strrchr(buffer,'.');
    if (ext) {
        ext[0] = '\0'; /* remove .ext from buffer */
    }
}

/* copies path removing name (NULL when if filename has no path) */
void get_streamfile_path(STREAMFILE* sf, char* buffer, size_t size) {
    const char* path;

    get_streamfile_name(sf, buffer, size);

    path = strrchr(buffer,DIR_SEPARATOR);
    if (path!=NULL) path = path+1; /* includes "/" */

    if (path) {
        buffer[path - buffer] = '\0';
    } else {
        buffer[0] = '\0';
    }
}

/* copies extension only */
void get_streamfile_ext(STREAMFILE* sf, char* buffer, size_t size) {
    char filename[PATH_LIMIT];
    const char* extension = NULL;

    get_streamfile_name(sf, filename, sizeof(filename));
    extension = filename_extension(filename);
    if (!extension) {
        buffer[0] = '\n';
    }
    else {
        strncpy(buffer, extension, size); //todo use something better
    }
}

/* ************************************************************************* */

/* debug util, mainly for custom IO testing */
void dump_streamfile(STREAMFILE* sf, int num) {
#ifdef VGM_DEBUG_OUTPUT
    off_t offset = 0;
    FILE* f = NULL;

    if (num >= 0) {
        char filename[PATH_LIMIT];
        char dumpname[PATH_LIMIT];

        get_streamfile_filename(sf, filename, PATH_LIMIT);
        snprintf(dumpname,PATH_LIMIT, "%s_%02i.dump", filename, num);

        f = fopen(dumpname,"wb");
        if (!f) return;
    }

    VGM_LOG("dump streamfile: size %x\n", get_streamfile_size(sf));
    while (offset < get_streamfile_size(sf)) {
        uint8_t buffer[0x8000];
        size_t read;

        read = read_streamfile(buffer,offset,0x8000,sf);
        if(!read) {
            VGM_LOG("dump streamfile: can't read at %lx\n", offset);
            break;
        }

        if (f)
            fwrite(buffer,sizeof(uint8_t),read, f);
        else
            VGM_LOGB(buffer,read,0);
        offset += read;
    }

    if (f) {
        fclose(f);
    }
#endif
}
