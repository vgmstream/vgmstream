#include "../streamfile.h"
#include "../util/vgmstream_limits.h"
#include "../util/log.h"
#include "../util/sf_utils.h"
#include "../vgmstream.h"


/* for dup/fdopen in some systems */
#ifndef _MSC_VER
    #include <unistd.h>
#endif

// for testing purposes; generally slower since reads often aren't optimized for unbuffered IO
//#define DISABLE_BUFFER

/* Enables a minor optimization when reopening file descriptors.
 * Some systems/compilers have issues though, and dupe'd FILEs may fread garbage data in rare cases,
 * possibly due to underlying buffers that get shared/thrashed by dup(). Seen for example in some .HPS and Ubi
 * bigfiles (some later MSVC versions) or PS2 .RSD (Mac), where 2nd channel = 2nd SF reads garbage at some points.
 *
 * Keep it for other systems since this is (probably) kinda useful, though a more sensible approach would be
 * redoing SF/FILE/buffer handling to avoid re-opening as much. */
#if !defined (_MSC_VER) && !defined (__ANDROID__) && !defined (__APPLE__)
    #define USE_STDIO_FDUP 1
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

#if defined(VGM_STDIO_UNICODE) && defined(WIN32)
    #undef fopen_v
    #define fopen_v fopen_win
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

#ifdef DISABLE_BUFFER
    if (offset != sf->offset) {
        fseek_v(sf->infile, offset, SEEK_SET);
    }
    read_total = fread(dst, sizeof(uint8_t), length, sf->infile);

    sf->offset = offset + read_total;
    return read_total;
#else
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

#if 0
        /* old workaround for USE_STDIO_FDUP bug, keep it here for a while as a reminder just in case */
        //fseek_v(sf->infile, ftell_v(sf->infile), SEEK_SET);
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
#endif
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

#ifdef USE_STDIO_FDUP
    /* minor optimization when reopening files, see comment in #define above */

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

    if (buf_size <= 0)
        buf_size = STREAMFILE_DEFAULT_BUFFER_SIZE;

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


#if defined(VGM_STDIO_UNICODE) && defined(WIN32)
static bool is_ascii(const char* str) {
    while (str[0] != 0x00) {
        uint8_t elem = (uint8_t)str[0];
        if (elem >= 0x80)
            return false;
        str++;
    }
    return true;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static FILE* fopen_win(const char* path, const char* mode) {
    // Micro-optimization since converting small-ish ascii strings is the common case.
    // Possibly irrelevant given that MultiByteToWideChar would do a full scan (but also extra stuff).
    if (is_ascii(path)) {
        return fopen(path, mode);
    }

    int done;
    wchar_t wpath[PATH_LIMIT];
    done = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, sizeof(wpath));
    if (done <= 0) return NULL;

    wchar_t wmode[3+1];
    done = MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, sizeof(mode));
    if (done <= 0) return NULL;

    return _wfopen(wpath, wmode);
}
#endif

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
    return open_stdio_streamfile_buffer(filename, 0);
}

STREAMFILE* open_stdio_streamfile_by_file(FILE* file, const char* filename) {
    return open_stdio_streamfile_buffer_by_file(file, filename, 0);
}


/* ************************************************************************* */

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

    VGM_LOG("dump streamfile %i: size %x\n", num, get_streamfile_size(sf));
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
        else if (num == -1)
            VGM_LOGB(buf, bytes, 0);
        //else: don't do anything (read test)
        offset += bytes;
    }

    if (f) {
        fclose(f);
    }
#endif
}
