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
} STDIOSTREAMFILE;

static STREAMFILE * open_stdio_streamfile_buffer(const char * const filename, size_t buffersize);
static STREAMFILE * open_stdio_streamfile_buffer_by_file(FILE *infile,const char * const filename, size_t buffersize);

static size_t read_stdio(STDIOSTREAMFILE *streamfile,uint8_t * dest, off_t offset, size_t length) {
    size_t length_read_total = 0;

    if (!streamfile || !dest || length <= 0 || offset < 0)
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
            VGM_ASSERT_ONCE(offset > streamfile->filesize, "STDIO: reading over filesize 0x%x @ 0x%"PRIx64" + 0x%x\n", streamfile->filesize, (off64_t)offset, length);
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
         * HPS is one format that is almost always affected by this. */
        fseek(streamfile->infile, ftell(streamfile->infile), SEEK_SET);
#endif

        /* fill the buffer (offset now is beyond buffer_offset) */
        streamfile->buffer_offset = offset;
        streamfile->validsize = fread(streamfile->buffer,sizeof(uint8_t),streamfile->buffersize,streamfile->infile);

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
static size_t get_size_stdio(STDIOSTREAMFILE * streamfile) {
    return streamfile->filesize;
}
static off_t get_offset_stdio(STDIOSTREAMFILE *streamfile) {
    return streamfile->offset;
}
static void get_name_stdio(STDIOSTREAMFILE *streamfile,char *buffer,size_t length) {
    strncpy(buffer,streamfile->name,length);
    buffer[length-1]='\0';
}
static void close_stdio(STDIOSTREAMFILE * streamfile) {
    fclose(streamfile->infile);
    free(streamfile->buffer);
    free(streamfile);
}

static STREAMFILE *open_stdio(STDIOSTREAMFILE *streamFile,const char * const filename,size_t buffersize) {
    int newfd;
    FILE *newfile;
    STREAMFILE *newstreamFile;

    if (!filename)
        return NULL;
#if !defined (__ANDROID__)
    // if same name, duplicate the file pointer we already have open
    if (!strcmp(streamFile->name,filename)) {
        if (((newfd = dup(fileno(streamFile->infile))) >= 0) &&
            (newfile = fdopen( newfd, "rb" ))) 
        {
            newstreamFile = open_stdio_streamfile_buffer_by_file(newfile,filename,buffersize);
            if (newstreamFile) { 
                return newstreamFile;
            }
            // failure, close it and try the default path (which will probably fail a second time)
            fclose(newfile);
        }
    }
#endif    
    // a normal open, open a new file
    return open_stdio_streamfile_buffer(filename,buffersize);
}

static STREAMFILE * open_stdio_streamfile_buffer_by_file(FILE *infile,const char * const filename, size_t buffersize) {
    uint8_t * buffer = NULL;
    STDIOSTREAMFILE * streamfile = NULL;

    buffer = calloc(buffersize,1);
    if (!buffer) goto fail;

    streamfile = calloc(1,sizeof(STDIOSTREAMFILE));
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

    strncpy(streamfile->name,filename,sizeof(streamfile->name));
    streamfile->name[sizeof(streamfile->name)-1] = '\0';

    /* cache filesize */
    fseeko(streamfile->infile,0,SEEK_END);
    streamfile->filesize = ftello(streamfile->infile);

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

static STREAMFILE * open_stdio_streamfile_buffer(const char * const filename, size_t buffersize) {
    FILE * infile;
    STREAMFILE *streamFile;

    infile = fopen(filename,"rb");
    if (!infile) return NULL;

    streamFile = open_stdio_streamfile_buffer_by_file(infile,filename,buffersize);
    if (!streamFile) {
        fclose(infile);
    }

    return streamFile;
}


STREAMFILE * open_stdio_streamfile(const char * filename) {
    return open_stdio_streamfile_buffer(filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE * open_stdio_streamfile_by_file(FILE * file, const char * filename) {
    return open_stdio_streamfile_buffer_by_file(file,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
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


static size_t buffer_read(BUFFER_STREAMFILE *streamfile, uint8_t * dest, off_t offset, size_t length) {
    size_t length_read_total = 0;

    if (!streamfile || !dest || length <= 0 || offset < 0)
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
            VGM_ASSERT_ONCE(offset > streamfile->filesize, "BUFFER: reading over filesize 0x%x @ 0x%"PRIx64" + 0x%x\n", streamfile->filesize, (off64_t)offset, length);
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
static size_t buffer_get_size(BUFFER_STREAMFILE * streamfile) {
    return streamfile->filesize; /* cache */
}
static size_t buffer_get_offset(BUFFER_STREAMFILE * streamfile) {
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

STREAMFILE *open_buffer_streamfile(STREAMFILE *streamfile, size_t buffer_size) {
    BUFFER_STREAMFILE *this_sf = NULL;

    if (!streamfile) goto fail;

    this_sf = calloc(1,sizeof(BUFFER_STREAMFILE));
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

/* **************************************************** */

//todo stream_index: copy? pass? funtion? external?
//todo use realnames on reopen? simplify?
//todo use safe string ops, this ain't easy

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
} WRAP_STREAMFILE;

static size_t wrap_read(WRAP_STREAMFILE *streamfile, uint8_t * dest, off_t offset, size_t length) {
    return streamfile->inner_sf->read(streamfile->inner_sf, dest, offset, length); /* default */
}
static size_t wrap_get_size(WRAP_STREAMFILE * streamfile) {
    return streamfile->inner_sf->get_size(streamfile->inner_sf); /* default */
}
static size_t wrap_get_offset(WRAP_STREAMFILE * streamfile) {
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

STREAMFILE *open_wrap_streamfile(STREAMFILE *streamfile) {
    WRAP_STREAMFILE *this_sf;

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

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
    off_t start;
    size_t size;
} CLAMP_STREAMFILE;

static size_t clamp_read(CLAMP_STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length) {
    off_t inner_offset = streamfile->start + offset;
    size_t clamp_length = length > (streamfile->size - offset) ? (streamfile->size - offset) : length;
    return streamfile->inner_sf->read(streamfile->inner_sf, dest, inner_offset, clamp_length);
}
static size_t clamp_get_size(CLAMP_STREAMFILE *streamfile) {
    return streamfile->size;
}
static off_t clamp_get_offset(CLAMP_STREAMFILE *streamfile) {
    return streamfile->inner_sf->get_offset(streamfile->inner_sf) - streamfile->start;
}
static void clamp_get_name(CLAMP_STREAMFILE *streamfile, char *buffer, size_t length) {
    streamfile->inner_sf->get_name(streamfile->inner_sf, buffer, length); /* default */
}
static STREAMFILE *clamp_open(CLAMP_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    char original_filename[PATH_LIMIT];
    STREAMFILE *new_inner_sf;

    new_inner_sf = streamfile->inner_sf->open(streamfile->inner_sf,filename,buffersize);
    streamfile->inner_sf->get_name(streamfile->inner_sf, original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) {
        return open_clamp_streamfile(new_inner_sf, streamfile->start, streamfile->size); /* clamp again */
    } else {
        return new_inner_sf; /**/
    }
}
static void clamp_close(CLAMP_STREAMFILE *streamfile) {
    streamfile->inner_sf->close(streamfile->inner_sf);
    free(streamfile);
}

STREAMFILE *open_clamp_streamfile(STREAMFILE *streamfile, off_t start, size_t size) {
    CLAMP_STREAMFILE *this_sf;

    if (!streamfile || !size) return NULL;
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

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
    void* data; /* state for custom reads, malloc'ed + copied on open (to re-open streamfiles cleanly) */
    size_t data_size;
    size_t (*read_callback)(STREAMFILE *, uint8_t *, off_t, size_t, void*); /* custom read to modify data before copying into buffer */
    size_t (*size_callback)(STREAMFILE *, void*); /* size when custom reads make data smaller/bigger than underlying streamfile */
    //todo would need to make sure re-opened streamfiles work with this, maybe should use init_data_callback per call
    //size_t (*close_data_callback)(STREAMFILE *, void*); /* called during close, allows to free stuff in data */
} IO_STREAMFILE;

static size_t io_read(IO_STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length) {
    return streamfile->read_callback(streamfile->inner_sf, dest, offset, length, streamfile->data);
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
static STREAMFILE *io_open(IO_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
    //todo should have some flag to decide if opening other files with IO
    STREAMFILE *new_inner_sf = streamfile->inner_sf->open(streamfile->inner_sf,filename,buffersize);
    return open_io_streamfile(new_inner_sf, streamfile->data, streamfile->data_size, streamfile->read_callback, streamfile->size_callback);
}
static void io_close(IO_STREAMFILE *streamfile) {
    streamfile->inner_sf->close(streamfile->inner_sf);
    free(streamfile->data);
    free(streamfile);
}

STREAMFILE *open_io_streamfile(STREAMFILE *streamfile, void* data, size_t data_size, void* read_callback, void* size_callback) {
    IO_STREAMFILE *this_sf;

    if (!streamfile) return NULL;
    if ((data && !data_size) || (!data && data_size)) return NULL;

    this_sf = calloc(1,sizeof(IO_STREAMFILE));
    if (!this_sf) return NULL;

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
        if (!this_sf->data)  {
            free(this_sf);
            return NULL;
        }
        memcpy(this_sf->data, data, data_size);
    }
    this_sf->data_size = data_size;
    this_sf->read_callback = read_callback;
    this_sf->size_callback = size_callback;

    return &this_sf->sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE sf;

    STREAMFILE *inner_sf;
    char fakename[PATH_LIMIT];
} FAKENAME_STREAMFILE;

static size_t fakename_read(FAKENAME_STREAMFILE *streamfile, uint8_t * dest, off_t offset, size_t length) {
    return streamfile->inner_sf->read(streamfile->inner_sf, dest, offset, length); /* default */
}
static size_t fakename_get_size(FAKENAME_STREAMFILE * streamfile) {
    return streamfile->inner_sf->get_size(streamfile->inner_sf); /* default */
}
static size_t fakename_get_offset(FAKENAME_STREAMFILE * streamfile) {
    return streamfile->inner_sf->get_offset(streamfile->inner_sf); /* default */
}
static void fakename_get_name(FAKENAME_STREAMFILE *streamfile, char *buffer, size_t length) {
    strncpy(buffer,streamfile->fakename,length);
    buffer[length-1]='\0';
}
static STREAMFILE *fakename_open(FAKENAME_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
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

STREAMFILE *open_fakename_streamfile(STREAMFILE *streamfile, const char * fakename, const char* fakeext) {
    FAKENAME_STREAMFILE *this_sf;

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
        char * ext = strrchr(this_sf->fakename,'.');
        if (ext != NULL)
            ext[1] = '\0'; /* truncate past dot */
        strcat(this_sf->fakename, fakeext);
    }

    return &this_sf->sf;
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

static size_t multifile_read(MULTIFILE_STREAMFILE *streamfile, uint8_t * dest, off_t offset, size_t length) {
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
        done += streamfile->inner_sfs[segment]->read(streamfile->inner_sfs[segment], dest+done, segment_offset, length - done);
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
static STREAMFILE *multifile_open(MULTIFILE_STREAMFILE *streamfile, const char * const filename, size_t buffersize) {
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

STREAMFILE *open_multifile_streamfile(STREAMFILE **streamfiles, size_t streamfiles_size) {
    MULTIFILE_STREAMFILE *this_sf;
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

/* **************************************************** */

STREAMFILE * open_streamfile(STREAMFILE *streamFile, const char * pathname) {
    return streamFile->open(streamFile,pathname,STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE * open_streamfile_by_ext(STREAMFILE *streamFile, const char * ext) {
    char filename_ext[PATH_LIMIT];

    streamFile->get_name(streamFile,filename_ext,sizeof(filename_ext));
    strcpy(filename_ext + strlen(filename_ext) - strlen(filename_extension(filename_ext)), ext);

    return streamFile->open(streamFile,filename_ext,STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE * open_streamfile_by_filename(STREAMFILE *streamFile, const char * name) {
    char foldername[PATH_LIMIT];
    char filename[PATH_LIMIT];
    const char *path;

    streamFile->get_name(streamFile,foldername,sizeof(foldername));

    path = strrchr(foldername,DIR_SEPARATOR);
    if (path!=NULL) path = path+1;

    if (path) {
        strcpy(filename, foldername);
        filename[path-foldername] = '\0';
        strcat(filename, name);
    } else {
        strcpy(filename, name);
    }

    return streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
}


/* Read a line into dst. The source files are lines separated by CRLF (Windows) / LF (Unux) / CR (Mac).
 * The line will be null-terminated and CR/LF removed if found.
 *
 * Returns the number of bytes read (including CR/LF), note that this is not the string length.
 * line_done_ptr is set to 1 if the complete line was read into dst; NULL can be passed to ignore.
 */
size_t get_streamfile_text_line(int dst_length, char * dst, off_t offset, STREAMFILE * streamfile, int *line_done_ptr) {
    int i;
    off_t file_length = get_streamfile_size(streamfile);
    int extra_bytes = 0; /* how many bytes over those put in the buffer were read */

    if (line_done_ptr) *line_done_ptr = 0;

    for (i = 0; i < dst_length-1 && offset+i < file_length; i++) {
        char in_char = read_8bit(offset+i,streamfile);
        /* check for end of line */
        if (in_char == 0x0d && read_8bit(offset+i+1,streamfile) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            if (line_done_ptr) *line_done_ptr = 1;
            break;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            if (line_done_ptr) *line_done_ptr = 1;
            break;
        }

        dst[i] = in_char;
    }

    dst[i] = '\0';

    /* did we fill the buffer? */
    if (i == dst_length) {
        char in_char = read_8bit(offset+i,streamfile);
        /* did the bytes we missed just happen to be the end of the line? */
        if (in_char == 0x0d && read_8bit(offset+i+1,streamfile) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            if (line_done_ptr) *line_done_ptr = 1;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            if (line_done_ptr) *line_done_ptr = 1;
        }
    }

    /* did we hit the file end? */
    if (offset+i == file_length) {
        /* then we did in fact finish reading the last line */
        if (line_done_ptr) *line_done_ptr = 1;
    }

    return i + extra_bytes;
}


/* reads a c-string (ANSI only), up to maxsize or NULL, returning size. buf is optional (works as get_string_size). */
size_t read_string(char * buf, size_t maxsize, off_t offset, STREAMFILE *streamFile) {
    size_t pos;

    for (pos = 0; pos < maxsize; pos++) {
        char c = read_8bit(offset + pos, streamFile);
        if (buf) buf[pos] = c;
        if (c == '\0')
            return pos;
        if (pos+1 == maxsize) { /* null at maxsize and don't validate (expected to be garbage) */
            if (buf) buf[pos] = '\0';
            return maxsize;
        }
        if (c < 0x20 || c > 0xA5)
            goto fail;
    }

fail:
    if (buf) buf[0] = '\0';
    return 0;
}


/* Opens a file containing decryption keys and copies to buffer.
 * Tries combinations of keynames based on the original filename.
 * returns size of key if found and copied */
size_t read_key_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile) {
    char keyname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    const char *path, *ext;
    STREAMFILE * streamFileKey = NULL;
    size_t keysize;

    streamFile->get_name(streamFile,filename,sizeof(filename));

    if (strlen(filename)+4 > sizeof(keyname)) goto fail;

    /* try to open a keyfile using variations:
     *  "(name.ext)key" (per song), "(.ext)key" (per folder) */
    {
        ext = strrchr(filename,'.');
        if (ext!=NULL) ext = ext+1;

        path = strrchr(filename,DIR_SEPARATOR);
        if (path!=NULL) path = path+1;

        /* "(name.ext)key" */
        strcpy(keyname, filename);
        strcat(keyname, "key");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;

        /* "(name.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;
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
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;

        /* "(.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;
        */

        goto fail;
    }

found:
    keysize = get_streamfile_size(streamFileKey);
    if (keysize > bufsize) goto fail;

    if (read_streamfile(buf, 0, keysize, streamFileKey) != keysize)
        goto fail;

    close_streamfile(streamFileKey);
    return keysize;

fail:
    close_streamfile(streamFileKey);
    return 0;
}


/**
 * Checks if the stream filename is one of the extensions (comma-separated, ex. "adx" or "adx,aix").
 * Empty is ok to accept files without extension ("", "adx,,aix"). Returns 0 on failure
 */
int check_extensions(STREAMFILE *streamFile, const char * cmp_exts) {
    char filename[PATH_LIMIT];
    const char * ext = NULL;
    const char * cmp_ext = NULL;
    const char * ststr_res = NULL;
    size_t ext_len, cmp_len;

    streamFile->get_name(streamFile,filename,sizeof(filename));
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


/**
 * Find a chunk starting from an offset, and save its offset/size (if not NULL), with offset after id/size.
 * Works for chunked headers in the form of "chunk_id chunk_size (data)"xN  (ex. RIFF).
 * The start_offset should be the first actual chunk (not "RIFF" or "WAVE" but "fmt ").
 * "full_chunk_size" signals chunk_size includes 4+4+data.
 *
 * returns 0 on failure
 */
int find_chunk_be(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(streamFile, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 1, 0);
}
int find_chunk_le(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(streamFile, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 0, 0);
}
int find_chunk(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int size_big_endian, int zero_size_end) {
    size_t filesize;
    off_t current_chunk = start_offset;

    filesize = get_streamfile_size(streamFile);
    /* read chunks */
    while (current_chunk < filesize) {
        uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
        off_t chunk_size = size_big_endian ?
                read_32bitBE(current_chunk+4,streamFile) :
                read_32bitLE(current_chunk+4,streamFile);

        if (chunk_type == chunk_id) {
            if (out_chunk_offset) *out_chunk_offset = current_chunk+8;
            if (out_chunk_size) *out_chunk_size = chunk_size;
            return 1;
        }

        /* empty chunk with 0 size, seen in some formats (XVAG uses it as end marker, Wwise doesn't) */
        if (chunk_size == 0 && zero_size_end)
            return 0;

        current_chunk += full_chunk_size ? chunk_size : 4+4+chunk_size;
    }

    return 0;
}

/* copies name as-is (may include full path included) */
void get_streamfile_name(STREAMFILE *streamFile, char * buffer, size_t size) {
    streamFile->get_name(streamFile,buffer,size);
}
/* copies the filename without path */
void get_streamfile_filename(STREAMFILE *streamFile, char * buffer, size_t size) {
    char foldername[PATH_LIMIT];
    const char *path;


    streamFile->get_name(streamFile,foldername,sizeof(foldername));

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
void get_streamfile_basename(STREAMFILE *streamFile, char * buffer, size_t size) {
    char *ext;

    get_streamfile_filename(streamFile,buffer,size);

    ext = strrchr(buffer,'.');
    if (ext) {
        ext[0] = '\0'; /* remove .ext from buffer */
    }
}
/* copies path removing name (NULL when if filename has no path) */
void get_streamfile_path(STREAMFILE *streamFile, char * buffer, size_t size) {
    const char *path;

    streamFile->get_name(streamFile,buffer,size);

    path = strrchr(buffer,DIR_SEPARATOR);
    if (path!=NULL) path = path+1; /* includes "/" */

    if (path) {
        buffer[path - buffer] = '\0';
    } else {
        buffer[0] = '\0';
    }
}
void get_streamfile_ext(STREAMFILE *streamFile, char * filename, size_t size) {
    streamFile->get_name(streamFile,filename,size);
    strcpy(filename, filename_extension(filename));
}

/* debug util, mainly for custom IO testing */
void dump_streamfile(STREAMFILE *streamFile, const char* out) {
#ifdef VGM_DEBUG_OUTPUT
    off_t offset = 0;
    FILE *f = NULL;

    if (out) {
        f = fopen(out,"wb");
        if (!f) return;
    }

    VGM_LOG("dump streamfile, size: %x\n", get_streamfile_size(streamFile));
    while (offset < get_streamfile_size(streamFile)) {
        uint8_t buffer[0x8000];
        size_t read;

        read = read_streamfile(buffer,offset,0x8000,streamFile);
        if (out)
            fwrite(buffer,sizeof(uint8_t),read, f);
        else
            VGM_LOGB(buffer,read,0);
        offset += read;
    }

    if (out) {
        fclose(f);
    }
#endif
}
