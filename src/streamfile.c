#include "streamfile.h"
#include "util.h"
#include "vgmstream.h"
#include <string.h>

/* for dup/fdopen in some systems */
#ifndef _MSC_VER
    #include <unistd.h>
#endif

//TODO: move
#ifndef DIR_SEPARATOR
    #if defined (_WIN32) || defined (WIN32)
        #define DIR_SEPARATOR '\\'
    #else
        #define DIR_SEPARATOR '/'
    #endif
#endif

/* For (rarely needed) +2GB file support we use fseek64/ftell64. Those are usually available
 * but may depend on compiler.
 * - MSVC: +VS2008 should work
 * - GCC/MingW: should be available
 * - GCC/Linux: should be available but some systems may need __USE_FILE_OFFSET64,
 *   that we (probably) don't want since that turns off_t to off64_t
 * - Clang: seems only defined on Linux/GNU environments, somehow emscripten is out
 *   (unsure about Clang Win since apparently they define _MSC_VER)
 * - Android: API +24 if not using __USE_FILE_OFFSET64
 * Not sure if fopen64 is needed in some cases.
 */

#if defined(_MSC_VER) //&& defined(__MSVCRT__)
    /* MSVC fixes (MinG64 seems to set MSVCRT too, but we want it below) */
    #include <io.h>

    #define fopen_v fopen
    #if (_MSC_VER >= 1400)
        #define fseek_v _fseeki64
        #define ftell_v _ftelli64
    #else
        #define fseek_v fseek
        #define ftell_v ftell
    #endif

    #ifdef fileno
        #undef fileno
    #endif
    #define fileno _fileno
    #define fdopen _fdopen
    #define dup _dup

    //#ifndef off64_t
    //    #define off_t/off64_t __int64
    //#endif

#elif defined(VGMSTREAM_USE_IO64) || defined(__MINGW32__) || defined(__MINGW64__)
    /* force, or known to work */
    #define fopen_v fopen
    #define fseek_v fseeko64  //fseeko
    #define ftell_v ftello64  //ftello

#elif defined(XBMC) || defined(__EMSCRIPTEN__) || defined (__ANDROID__)
    /* may depend on version */
    #define fopen_v fopen
    #define fseek_v fseek
    #define ftell_v ftell

#else
    /* other Linux systems may already use off64_t in fseeko/ftello? */
    #define fopen_v fopen
    #define fseek_v fseeko
    #define ftell_v ftello
#endif


/* a STREAMFILE that operates via standard IO using a buffer */
typedef struct {
    STREAMFILE vt;          /* callbacks */

    FILE* infile;           /* actual FILE */
    char name[PATH_LIMIT];  /* FILE filename */
    int name_len;           /* cache */
    offv_t offset;          /* last read offset (info) */
    offv_t buf_offset;      /* current buffer data start */
    uint8_t* buf;           /* data buffer */
    size_t buf_size;        /* max buffer size */
    size_t valid_size;      /* current buffer size */
    size_t file_size;       /* buffered file size */
} STDIO_STREAMFILE;

static STREAMFILE* open_stdio_streamfile_buffer(const char* const filename, size_t buf_size);
static STREAMFILE* open_stdio_streamfile_buffer_by_file(FILE *infile, const char* const filename, size_t buf_size);

static size_t stdio_read(STDIO_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    size_t read_total = 0;

    if (/*!sf->infile ||*/ !dst || length <= 0 || offset < 0)
        return 0;

    //;VGM_LOG("stdio: read %lx + %x (buf %lx + %x)\n", offset, length, sf->buf_offset, sf->valid_size, sf->buf_size);

    /* is the part of the requested length in the buffer? */
    if (offset >= sf->buf_offset && offset < sf->buf_offset + sf->valid_size) {
        size_t buf_limit;
        int buf_into = (int)(offset - sf->buf_offset);

        buf_limit = sf->valid_size - buf_into;
        if (buf_limit > length)
            buf_limit = length;

        //;VGM_LOG("stdio: copy buf %lx + %x (+ %x) (buf %lx + %x)\n", offset, length_to_read, (length - length_to_read), sf->buf_offset, sf->valid_size);

        memcpy(dst, sf->buf + buf_into, buf_limit);
        read_total += buf_limit;
        length -= buf_limit;
        offset += buf_limit;
        dst += buf_limit;
    }

#ifdef VGM_DEBUG_OUTPUT
    if (offset < sf->buf_offset && length > 0) {
        //VGM_LOG("stdio: rebuffer, requested %x vs %x (sf %x)\n", (uint32_t)offset, (uint32_t)sf->buf_offset, (uint32_t)sf);
        //sf->rebuffer++;
        //if (rebuffer > N) ...
    }
#endif

    /* possible if all data was copied to buf and FD closed */
    if (!sf->infile)
        return read_total;

    /* read the rest of the requested length */
    while (length > 0) {
        size_t length_to_read;

        /* ignore requests at EOF */
        if (offset >= sf->file_size) {
            //offset = sf->file_size; /* seems fseek doesn't clamp offset */
            VGM_ASSERT_ONCE(offset > sf->file_size, "STDIO: reading over file_size 0x%x @ 0x%x + 0x%x\n", sf->file_size, (uint32_t)offset, length);
            break;
        }

        /* position to new offset */
        if (fseek_v(sf->infile, offset, SEEK_SET)) {
            break; /* this shouldn't happen in our code */
        }

#ifdef _MSC_VER
        /* Workaround a bug that appears when compiling with MSVC (later versions).
         * This bug is deterministic and seemingly appears randomly after seeking.
         * It results in fread returning data from the wrong area of the file.
         * HPS is one format that is almost always affected by this.
         * May be related/same as stdio_open's fixed bug when using dup(), try disabling */
        fseek_v(sf->infile, ftell_v(sf->infile), SEEK_SET);
#endif

        /* fill the buffer (offset now is beyond buf_offset) */
        sf->buf_offset = offset;
        sf->valid_size = fread(sf->buf, sizeof(uint8_t), sf->buf_size, sf->infile);
        //;VGM_LOG("stdio: read buf %lx + %x\n", sf->buf_offset, sf->valid_size);

        /* decide how much must be read this time */
        if (length > sf->buf_size)
            length_to_read = sf->buf_size;
        else
            length_to_read = length;

        /* give up on partial reads (EOF) */
        if (sf->valid_size < length_to_read) {
            memcpy(dst, sf->buf, sf->valid_size);
            offset += sf->valid_size;
            read_total += sf->valid_size;
            break;
        }

        /* use the new buffer */
        memcpy(dst, sf->buf, length_to_read);
        offset += length_to_read;
        read_total += length_to_read;
        length -= length_to_read;
        dst += length_to_read;
    }

    sf->offset = offset; /* last fread offset */
    return read_total;
}

static size_t stdio_get_size(STDIO_STREAMFILE* sf) {
    return sf->file_size;
}

static offv_t stdio_get_offset(STDIO_STREAMFILE* sf) {
    return sf->offset;
}

static void stdio_get_name(STDIO_STREAMFILE* sf, char* name, size_t name_size) {
    int copy_size = sf->name_len + 1;
    if (copy_size > name_size)
        copy_size = name_size;

    memcpy(name, sf->name, copy_size);
    name[copy_size - 1] = '\0';
}

static STREAMFILE* stdio_open(STDIO_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    if (!filename)
        return NULL;

#if !defined (__ANDROID__) && !defined (_MSC_VER)
    /* when enabling this for MSVC it'll seemingly work, but there are issues possibly related to underlying
     * IO buffers when using dup(), noticeable by re-opening the same streamfile with small buffer sizes
     * (reads garbage). fseek bug in line 81 may be related/same thing and may be removed.
     * this reportedly this causes issues in Android too */

    /* if same name, duplicate the file descriptor we already have open */
    if (sf->infile && !strcmp(sf->name,filename)) {
        int new_fd;
        FILE *new_file = NULL;

        if (((new_fd = dup(fileno(sf->infile))) >= 0) && (new_file = fdopen(new_fd, "rb")))  {
            STREAMFILE* new_sf = open_stdio_streamfile_buffer_by_file(new_file, filename, buf_size);
            if (new_sf)
                return new_sf;
            fclose(new_file);
        }
        if (new_fd >= 0 && !new_file)
            close(new_fd); /* fdopen may fail when opening too many files */

        /* on failure just close and try the default path (which will probably fail a second time) */
    }
#endif

    return open_stdio_streamfile_buffer(filename, buf_size);
}

static void stdio_close(STDIO_STREAMFILE* sf) {
    if (sf->infile)
        fclose(sf->infile);
    free(sf->buf);
    free(sf);
}


static STREAMFILE* open_stdio_streamfile_buffer_by_file(FILE* infile, const char* const filename, size_t buf_size) {
    uint8_t* buf = NULL;
    STDIO_STREAMFILE* this_sf = NULL;

    buf = calloc(buf_size, sizeof(uint8_t));
    if (!buf) goto fail;

    this_sf = calloc(1, sizeof(STDIO_STREAMFILE));
    if (!this_sf) goto fail;

    this_sf->vt.read = (void*)stdio_read;
    this_sf->vt.get_size = (void*)stdio_get_size;
    this_sf->vt.get_offset = (void*)stdio_get_offset;
    this_sf->vt.get_name = (void*)stdio_get_name;
    this_sf->vt.open = (void*)stdio_open;
    this_sf->vt.close = (void*)stdio_close;

    this_sf->infile = infile;
    this_sf->buf_size = buf_size;
    this_sf->buf = buf;

    this_sf->name_len = strlen(filename);
    if (this_sf->name_len >= sizeof(this_sf->name))
        goto fail;
    memcpy(this_sf->name, filename, this_sf->name_len);
    this_sf->name[this_sf->name_len] = '\0';

    /* cache file_size */
    if (infile) {
        fseek_v(this_sf->infile, 0x00, SEEK_END);
        this_sf->file_size = ftell_v(this_sf->infile);
        fseek_v(this_sf->infile, 0x00, SEEK_SET);
    }
    else {
        this_sf->file_size = 0; /* allow virtual, non-existing files */
    }

    /* Typically fseek(o)/ftell(o) may only handle up to ~2.14GB, signed 32b = 0x7FFFFFFF (rarely
     * happens in giant banks like FSB/KTSR). Should work if configured properly using ftell_v, log otherwise. */
    if (this_sf->file_size == 0xFFFFFFFF) { /* -1 on error */
        vgm_logi("STREAMFILE: file size too big (report)\n");
        goto fail; /* can be ignored but may result in strange/unexpected behaviors */
    }

    /* Rarely a TXTP needs to open *many* streamfiles = many file descriptors = reaches OS limit = error.
     * Ideally should detect better and open/close as needed or reuse FDs for files that don't play at
     * the same time, but it's complex since every SF is separate (would need some kind of FD manager).
     * For the time being, if the file is smaller that buffer we can just read it fully and close the FD,
     * that should help since big TXTP usually just need many small files.
     * Doubles as an optimization as most files given will be read fully into buf on first read. */
    if (this_sf->file_size && this_sf->file_size < this_sf->buf_size && this_sf->infile) {
        //;VGM_LOG("stdio: fit filesize %x into buf %x\n", sf->file_size, sf->buf_size);

        this_sf->buf_offset = 0;
        this_sf->valid_size = fread(this_sf->buf, sizeof(uint8_t), this_sf->file_size, this_sf->infile);

        fclose(this_sf->infile);
        this_sf->infile = NULL;
    }

    return &this_sf->vt;

fail:
    free(buf);
    free(this_sf);
    return NULL;
}

static STREAMFILE* open_stdio_streamfile_buffer(const char* const filename, size_t bufsize) {
    FILE* infile = NULL;
    STREAMFILE* sf = NULL;

    infile = fopen_v(filename,"rb");
    if (!infile) {
        /* allow non-existing files in some cases */
        if (!vgmstream_is_virtual_filename(filename))
            return NULL;
    }

    sf = open_stdio_streamfile_buffer_by_file(infile, filename, bufsize);
    if (!sf) {
        if (infile) fclose(infile);
    }

    return sf;
}

STREAMFILE* open_stdio_streamfile(const char* filename) {
    return open_stdio_streamfile_buffer(filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE* open_stdio_streamfile_by_file(FILE* file, const char* filename) {
    return open_stdio_streamfile_buffer_by_file(file, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
}

/* **************************************************** */

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    offv_t offset;          /* last read offset (info) */
    offv_t buf_offset;      /* current buffer data start */
    uint8_t* buf;           /* data buffer */
    size_t buf_size;        /* max buffer size */
    size_t valid_size;      /* current buffer size */
    size_t file_size;       /* buffered file size */
} BUFFER_STREAMFILE;


static size_t buffer_read(BUFFER_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    size_t read_total = 0;

    if (!dst || length <= 0 || offset < 0)
        return 0;

    /* is the part of the requested length in the buffer? */
    if (offset >= sf->buf_offset && offset < sf->buf_offset + sf->valid_size) {
        size_t buf_limit;
        int buf_into = (int)(offset - sf->buf_offset);

        buf_limit = sf->valid_size - buf_into;
        if (buf_limit > length)
            buf_limit = length;

        memcpy(dst, sf->buf + buf_into, buf_limit);
        read_total += buf_limit;
        length -= buf_limit;
        offset += buf_limit;
        dst += buf_limit;
    }

#ifdef VGM_DEBUG_OUTPUT
    if (offset < sf->buf_offset) {
        //VGM_LOG("buffer: rebuffer, requested %x vs %x (sf %x)\n", (uint32_t)offset, (uint32_t)sf->buf_offset, (uint32_t)sf);
    }
#endif

    /* read the rest of the requested length */
    while (length > 0) {
        size_t buf_limit;

        /* ignore requests at EOF */
        if (offset >= sf->file_size) {
            //offset = sf->file_size; /* seems fseek doesn't clamp offset */
            VGM_ASSERT_ONCE(offset > sf->file_size, "buffer: reading over file_size 0x%x @ 0x%x + 0x%x\n", sf->file_size, (uint32_t)offset, length);
            break;
        }

        /* fill the buffer (offset now is beyond buf_offset) */
        sf->buf_offset = offset;
        sf->valid_size = sf->inner_sf->read(sf->inner_sf, sf->buf, sf->buf_offset, sf->buf_size);

        /* decide how much must be read this time */
        if (length > sf->buf_size)
            buf_limit = sf->buf_size;
        else
            buf_limit = length;

        /* give up on partial reads (EOF) */
        if (sf->valid_size < buf_limit) {
            memcpy(dst, sf->buf, sf->valid_size);
            offset += sf->valid_size;
            read_total += sf->valid_size;
            break;
        }

        /* use the new buffer */
        memcpy(dst, sf->buf, buf_limit);
        offset += buf_limit;
        read_total += buf_limit;
        length -= buf_limit;
        dst += buf_limit;
    }

    sf->offset = offset; /* last fread offset */
    return read_total;
}
static size_t buffer_get_size(BUFFER_STREAMFILE* sf) {
    return sf->file_size; /* cache */
}
static offv_t buffer_get_offset(BUFFER_STREAMFILE* sf) {
    return sf->offset; /* cache */
}
static void buffer_get_name(BUFFER_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* buffer_open(BUFFER_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    STREAMFILE* new_inner_sf = sf->inner_sf->open(sf->inner_sf,filename,buf_size);
    return open_buffer_streamfile(new_inner_sf, buf_size); /* original buffer size is preferable? */
}

static void buffer_close(BUFFER_STREAMFILE* sf) {
    sf->inner_sf->close(sf->inner_sf);
    free(sf->buf);
    free(sf);
}

STREAMFILE* open_buffer_streamfile(STREAMFILE* sf, size_t buf_size) {
    uint8_t* buf = NULL;
    BUFFER_STREAMFILE* this_sf = NULL;

    if (!sf) goto fail;

    if (buf_size == 0)
        buf_size = STREAMFILE_DEFAULT_BUFFER_SIZE;

    buf = calloc(buf_size, sizeof(uint8_t));
    if (!buf) goto fail;

    this_sf = calloc(1, sizeof(BUFFER_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)buffer_read;
    this_sf->vt.get_size = (void*)buffer_get_size;
    this_sf->vt.get_offset = (void*)buffer_get_offset;
    this_sf->vt.get_name = (void*)buffer_get_name;
    this_sf->vt.open = (void*)buffer_open;
    this_sf->vt.close = (void*)buffer_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;
    this_sf->buf_size = buf_size;
    this_sf->buf = buf;

    this_sf->file_size = sf->get_size(sf);

    return &this_sf->vt;

fail:
    if (this_sf) free(this_sf->buf);
    free(this_sf);
    return NULL;
}
STREAMFILE* open_buffer_streamfile_f(STREAMFILE* sf, size_t buffer_size) {
    STREAMFILE* new_sf = open_buffer_streamfile(sf, buffer_size);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}

/* **************************************************** */

//todo stream_index: copy? pass? funtion? external?
//todo use realnames on reopen? simplify?
//todo use safe string ops, this ain't easy

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
} WRAP_STREAMFILE;

static size_t wrap_read(WRAP_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->inner_sf->read(sf->inner_sf, dst, offset, length); /* default */
}
static size_t wrap_get_size(WRAP_STREAMFILE* sf) {
    return sf->inner_sf->get_size(sf->inner_sf); /* default */
}
static offv_t wrap_get_offset(WRAP_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf); /* default */
}
static void wrap_get_name(WRAP_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* wrap_open(WRAP_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    return sf->inner_sf->open(sf->inner_sf, filename, buf_size); /* default (don't call open_wrap_streamfile) */
}

static void wrap_close(WRAP_STREAMFILE* sf) {
    //sf->inner_sf->close(sf->inner_sf); /* don't close */
    free(sf);
}

STREAMFILE* open_wrap_streamfile(STREAMFILE* sf) {
    WRAP_STREAMFILE* this_sf = NULL;

    if (!sf) return NULL;

    this_sf = calloc(1, sizeof(WRAP_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)wrap_read;
    this_sf->vt.get_size = (void*)wrap_get_size;
    this_sf->vt.get_offset = (void*)wrap_get_offset;
    this_sf->vt.get_name = (void*)wrap_get_name;
    this_sf->vt.open = (void*)wrap_open;
    this_sf->vt.close = (void*)wrap_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;

    return &this_sf->vt;
}
STREAMFILE* open_wrap_streamfile_f(STREAMFILE* sf) {
    STREAMFILE* new_sf = open_wrap_streamfile(sf);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    offv_t start;
    size_t size;
} CLAMP_STREAMFILE;

static size_t clamp_read(CLAMP_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    offv_t inner_offset = sf->start + offset;
    size_t clamp_length = length;

    if (offset + length > sf->size) {
        if (offset >= sf->size)
            clamp_length = 0;
        else
            clamp_length = sf->size - offset;
    }

    return sf->inner_sf->read(sf->inner_sf, dst, inner_offset, clamp_length);
}
static size_t clamp_get_size(CLAMP_STREAMFILE* sf) {
    return sf->size;
}
static offv_t clamp_get_offset(CLAMP_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf) - sf->start;
}
static void clamp_get_name(CLAMP_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* clamp_open(CLAMP_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    char original_filename[PATH_LIMIT];
    STREAMFILE* new_inner_sf = NULL;

    new_inner_sf = sf->inner_sf->open(sf->inner_sf,filename,buf_size);
    sf->inner_sf->get_name(sf->inner_sf, original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) {
        return open_clamp_streamfile(new_inner_sf, sf->start, sf->size); /* clamp again */
    } else {
        return new_inner_sf;
    }
}

static void clamp_close(CLAMP_STREAMFILE* sf) {
    sf->inner_sf->close(sf->inner_sf);
    free(sf);
}

STREAMFILE* open_clamp_streamfile(STREAMFILE* sf, offv_t start, size_t size) {
    CLAMP_STREAMFILE* this_sf = NULL;

    if (!sf || size == 0) return NULL;
    if (start + size > get_streamfile_size(sf)) return NULL;

    this_sf = calloc(1, sizeof(CLAMP_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)clamp_read;
    this_sf->vt.get_size = (void*)clamp_get_size;
    this_sf->vt.get_offset = (void*)clamp_get_offset;
    this_sf->vt.get_name = (void*)clamp_get_name;
    this_sf->vt.open = (void*)clamp_open;
    this_sf->vt.close = (void*)clamp_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;
    this_sf->start = start;
    this_sf->size = size;

    return &this_sf->vt;
}
STREAMFILE* open_clamp_streamfile_f(STREAMFILE* sf, offv_t start, size_t size) {
    STREAMFILE* new_sf = open_clamp_streamfile(sf, start, size);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    void* data; /* state for custom reads, malloc'ed + copied on open (to re-open streamfiles cleanly) */
    size_t data_size;
    size_t (*read_callback)(STREAMFILE*, uint8_t*, off_t, size_t, void*); /* custom read to modify data before copying into buffer */
    size_t (*size_callback)(STREAMFILE*, void*); /* size when custom reads make data smaller/bigger than underlying streamfile */
    int (*init_callback)(STREAMFILE*, void*); /* init the data struct members somehow, return >= 0 if ok */
    void (*close_callback)(STREAMFILE*, void*); /* close the data struct members somehow */
    /* read doesn't use offv_t since callbacks would need to be modified */
} IO_STREAMFILE;

static size_t io_read(IO_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->read_callback(sf->inner_sf, dst, (off_t)offset, length, sf->data);
}
static size_t io_get_size(IO_STREAMFILE* sf) {
    if (sf->size_callback)
        return sf->size_callback(sf->inner_sf, sf->data);
    else
        return sf->inner_sf->get_size(sf->inner_sf); /* default */
}
static offv_t io_get_offset(IO_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf);  /* default */
}
static void io_get_name(IO_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* io_open(IO_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    STREAMFILE* new_inner_sf = sf->inner_sf->open(sf->inner_sf,filename,buf_size);
    return open_io_streamfile_ex(new_inner_sf, sf->data, sf->data_size, sf->read_callback, sf->size_callback, sf->init_callback, sf->close_callback);
}

static void io_close(IO_STREAMFILE* sf) {
    if (sf->close_callback)
        sf->close_callback(sf->inner_sf, sf->data);
    sf->inner_sf->close(sf->inner_sf);
    free(sf->data);
    free(sf);
}

STREAMFILE* open_io_streamfile_ex(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback, void* init_callback, void* close_callback) {
    IO_STREAMFILE* this_sf = NULL;

    if (!sf) goto fail;
    if ((data && !data_size) || (!data && data_size)) goto fail;

    this_sf = calloc(1, sizeof(IO_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)io_read;
    this_sf->vt.get_size = (void*)io_get_size;
    this_sf->vt.get_offset = (void*)io_get_offset;
    this_sf->vt.get_name = (void*)io_get_name;
    this_sf->vt.open = (void*)io_open;
    this_sf->vt.close = (void*)io_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;
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

    return &this_sf->vt;

fail:
    if (this_sf) free(this_sf->data);
    free(this_sf);
    return NULL;
}

STREAMFILE* open_io_streamfile_ex_f(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback, void* init_callback, void* close_callback) {
    STREAMFILE* new_sf = open_io_streamfile_ex(sf, data, data_size, read_callback, size_callback, init_callback, close_callback);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}

STREAMFILE* open_io_streamfile(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback) {
    return open_io_streamfile_ex(sf, data, data_size, read_callback, size_callback, NULL, NULL);
}
STREAMFILE* open_io_streamfile_f(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback) {
    return open_io_streamfile_ex_f(sf, data, data_size, read_callback, size_callback, NULL, NULL);
}

/* **************************************************** */

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    char fakename[PATH_LIMIT];
    int fakename_len;
} FAKENAME_STREAMFILE;

static size_t fakename_read(FAKENAME_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->inner_sf->read(sf->inner_sf, dst, offset, length); /* default */
}
static size_t fakename_get_size(FAKENAME_STREAMFILE* sf) {
    return sf->inner_sf->get_size(sf->inner_sf); /* default */
}
static offv_t fakename_get_offset(FAKENAME_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf); /* default */
}
static void fakename_get_name(FAKENAME_STREAMFILE* sf, char* name, size_t name_size) {
    int copy_size = sf->fakename_len + 1;
    if (copy_size > name_size)
        copy_size = name_size;
    memcpy(name, sf->fakename, copy_size);
    name[copy_size - 1] = '\0';
}

static STREAMFILE* fakename_open(FAKENAME_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    /* detect re-opening the file */
    if (strcmp(filename, sf->fakename) == 0) {
        STREAMFILE* new_inner_sf;
        char original_filename[PATH_LIMIT];

        sf->inner_sf->get_name(sf->inner_sf, original_filename, PATH_LIMIT);
        new_inner_sf = sf->inner_sf->open(sf->inner_sf, original_filename, buf_size);
        return open_fakename_streamfile(new_inner_sf, sf->fakename, NULL);
    }
    else {
        return sf->inner_sf->open(sf->inner_sf, filename, buf_size);
    }
}
static void fakename_close(FAKENAME_STREAMFILE* sf) {
    sf->inner_sf->close(sf->inner_sf);
    free(sf);
}

STREAMFILE* open_fakename_streamfile(STREAMFILE* sf, const char* fakename, const char* fakeext) {
    FAKENAME_STREAMFILE* this_sf = NULL;

    if (!sf || (!fakename && !fakeext)) return NULL;

    this_sf = calloc(1, sizeof(FAKENAME_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)fakename_read;
    this_sf->vt.get_size = (void*)fakename_get_size;
    this_sf->vt.get_offset = (void*)fakename_get_offset;
    this_sf->vt.get_name = (void*)fakename_get_name;
    this_sf->vt.open = (void*)fakename_open;
    this_sf->vt.close = (void*)fakename_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;

    /* copy passed name or retain current, and swap extension if expected */
    if (fakename) {
        strcpy(this_sf->fakename, fakename);
    } else {
        sf->get_name(sf, this_sf->fakename, PATH_LIMIT);
    }

    if (fakeext) {
        char* ext = strrchr(this_sf->fakename, '.');
        if (ext != NULL) {
            ext[1] = '\0'; /* truncate past dot */
        } else {
            strcat(this_sf->fakename, "."); /* no extension = add dot */
        }
        strcat(this_sf->fakename, fakeext);
    }

    this_sf->fakename_len = strlen(this_sf->fakename);

    return &this_sf->vt;
}
STREAMFILE* open_fakename_streamfile_f(STREAMFILE* sf, const char* fakename, const char* fakeext) {
    STREAMFILE* new_sf = open_fakename_streamfile(sf, fakename, fakeext);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}

/* **************************************************** */

typedef struct {
    STREAMFILE vt;

    STREAMFILE** inner_sfs;
    size_t inner_sfs_size;
    size_t *sizes;
    offv_t size;
    offv_t offset;
} MULTIFILE_STREAMFILE;

static size_t multifile_read(MULTIFILE_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    int i, segment = 0;
    offv_t segment_offset = 0;
    size_t done = 0;

    if (offset > sf->size) {
        sf->offset = sf->size;
        return 0;
    }

    /* map external offset to multifile offset */
    for (i = 0; i < sf->inner_sfs_size; i++) {
        size_t segment_size = sf->sizes[i];
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
        if (segment >= sf->inner_sfs_size) /* over last segment, not fully done */
            break;
        /* reads over segment size are ok, will return smaller value and continue next segment */
        done += sf->inner_sfs[segment]->read(sf->inner_sfs[segment], dst + done, segment_offset, length - done);
        segment++;
        segment_offset = 0;
    }

    sf->offset = offset + done;
    return done;
}
static size_t multifile_get_size(MULTIFILE_STREAMFILE* sf) {
    return sf->size;
}
static offv_t multifile_get_offset(MULTIFILE_STREAMFILE* sf) {
    return sf->offset;
}
static void multifile_get_name(MULTIFILE_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sfs[0]->get_name(sf->inner_sfs[0], name, name_size);
}

static STREAMFILE* multifile_open(MULTIFILE_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    char original_filename[PATH_LIMIT];
    STREAMFILE* new_sf = NULL;
    STREAMFILE** new_inner_sfs = NULL;
    int i;

    sf->inner_sfs[0]->get_name(sf->inner_sfs[0], original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) { /* same multifile */
        new_inner_sfs = calloc(sf->inner_sfs_size, sizeof(STREAMFILE*));
        if (!new_inner_sfs) goto fail;

        for (i = 0; i < sf->inner_sfs_size; i++) {
            sf->inner_sfs[i]->get_name(sf->inner_sfs[i], original_filename, PATH_LIMIT);
            new_inner_sfs[i] = sf->inner_sfs[i]->open(sf->inner_sfs[i], original_filename, buf_size);
            if (!new_inner_sfs[i]) goto fail;
        }

        new_sf = open_multifile_streamfile(new_inner_sfs, sf->inner_sfs_size);
        if (!new_sf) goto fail;

        free(new_inner_sfs);
        return new_sf;
    }
    else {
        return sf->inner_sfs[0]->open(sf->inner_sfs[0], filename, buf_size); /* regular file */
    }

fail:
    if (new_inner_sfs) {
        for (i = 0; i < sf->inner_sfs_size; i++)
            close_streamfile(new_inner_sfs[i]);
    }
    free(new_inner_sfs);
    return NULL;
}
static void multifile_close(MULTIFILE_STREAMFILE* sf) {
    int i;
    for (i = 0; i < sf->inner_sfs_size; i++) {
        for (i = 0; i < sf->inner_sfs_size; i++) {
            close_streamfile(sf->inner_sfs[i]);
        }
    }
    free(sf->inner_sfs);
    free(sf->sizes);
    free(sf);
}

STREAMFILE* open_multifile_streamfile(STREAMFILE** sfs, size_t sfs_size) {
    MULTIFILE_STREAMFILE* this_sf = NULL;
    int i;

    if (!sfs || !sfs_size) return NULL;

    for (i = 0; i < sfs_size; i++) {
        if (!sfs[i]) return NULL;
    }

    this_sf = calloc(1, sizeof(MULTIFILE_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)multifile_read;
    this_sf->vt.get_size = (void*)multifile_get_size;
    this_sf->vt.get_offset = (void*)multifile_get_offset;
    this_sf->vt.get_name = (void*)multifile_get_name;
    this_sf->vt.open = (void*)multifile_open;
    this_sf->vt.close = (void*)multifile_close;
    this_sf->vt.stream_index = sfs[0]->stream_index;

    this_sf->inner_sfs_size = sfs_size;
    this_sf->inner_sfs = calloc(sfs_size, sizeof(STREAMFILE*));
    if (!this_sf->inner_sfs) goto fail;
    this_sf->sizes = calloc(sfs_size, sizeof(size_t));
    if (!this_sf->sizes) goto fail;

    for (i = 0; i < this_sf->inner_sfs_size; i++) {
        this_sf->inner_sfs[i] = sfs[i];
        this_sf->sizes[i] = sfs[i]->get_size(sfs[i]);
        this_sf->size += this_sf->sizes[i];
    }

    return &this_sf->vt;

fail:
    if (this_sf) {
        free(this_sf->inner_sfs);
        free(this_sf->sizes);
    }
    free(this_sf);
    return NULL;
}
STREAMFILE* open_multifile_streamfile_f(STREAMFILE** sfs, size_t sfs_size) {
    STREAMFILE* new_sf = open_multifile_streamfile(sfs, sfs_size);
    if (!new_sf) {
        int i;
        for (i = 0; i < sfs_size; i++) {
            close_streamfile(sfs[i]);
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

    get_streamfile_name(sf, filename, sizeof(filename));

    swap_extension(filename, sizeof(filename), ext);

    return open_streamfile(sf, filename);
}

STREAMFILE* open_streamfile_by_filename(STREAMFILE* sf, const char* filename) {
    char fullname[PATH_LIMIT];
    char partname[PATH_LIMIT];
    char *path, *name, *otherpath;

    if (!sf || !filename || !filename[0]) return NULL;

    get_streamfile_name(sf, fullname, sizeof(fullname));

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
            char* pathprev;

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

    return open_streamfile(sf, fullname);
}

STREAMFILE* reopen_streamfile(STREAMFILE* sf, size_t buffer_size) {
    char pathname[PATH_LIMIT];

    if (!sf) return NULL;

    if (buffer_size == 0)
        buffer_size = STREAMFILE_DEFAULT_BUFFER_SIZE;
    get_streamfile_name(sf, pathname, sizeof(pathname));
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
            /* better way? */
            if (strcmp(line, "#@reset-pos") == 0) {
                file_pos = 0;
            }
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

    sf->get_name(sf, filename, sizeof(filename));
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
    offv_t offset = 0;
    FILE* f = NULL;

    if (num >= 0) {
        char filename[PATH_LIMIT];
        char dumpname[PATH_LIMIT];

        get_streamfile_filename(sf, filename, sizeof(filename));
        snprintf(dumpname, sizeof(dumpname), "%s_%02i.dump", filename, num);

        f = fopen_v(dumpname,"wb");
        if (!f) return;
    }

    VGM_LOG("dump streamfile: size %x\n", get_streamfile_size(sf));
    while (offset < get_streamfile_size(sf)) {
        uint8_t buf[0x8000];
        size_t bytes;

        bytes = read_streamfile(buf, offset, sizeof(buf), sf);
        if(!bytes) {
            VGM_LOG("dump streamfile: can't read at %x\n", (uint32_t)offset);
            break;
        }

        if (f)
            fwrite(buf, sizeof(uint8_t), bytes, f);
        else
            VGM_LOGB(buf, bytes, 0);
        offset += bytes;
    }

    if (f) {
        fclose(f);
    }
#endif
}
