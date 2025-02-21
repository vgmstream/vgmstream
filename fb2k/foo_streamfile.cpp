#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <io.h>

#include <foobar2000/SDK/foobar2000.h>

extern "C" {
#include "../src/libvgmstream.h"
}
#include "foo_vgmstream.h"

/* Value can be adjusted freely but 8k is a good enough compromise. */
#define FOO_STREAMFILE_DEFAULT_BUFFER_SIZE 0x8000


/* a STREAMFILE that operates via foobar's file service using a buffer */
typedef struct {
    bool m_file_opened;         /* if foobar IO service opened the file */
    service_ptr_t<file> m_file; /* foobar IO service */
    abort_callback* p_abort;    /* foobar error stuff */

    char* name;                 /* IO filename */
    int name_len;               /* cache */
    char* archname;             /* for foobar's foo_unpack archives */
    int archname_len;           /* cache */
    int archpath_end;           /* where the last \ ends before archive name */
    int archfile_end;           /* where the last | ends before file name */

    int64_t offset;             /* last read offset (info) */
    int64_t buf_offset;         /* current buffer data start */
    uint8_t* buf;               /* data buffer */
    size_t buf_size;            /* max buffer size */
    size_t valid_size;          /* current buffer size */
    size_t file_size;           /* buffered file size */
} foo_priv_t;

static libstreamfile_t* open_foo_streamfile_internal(const char* const filename, abort_callback* p_abort, t_filestats* stats);
static libstreamfile_t* open_foo_streamfile_from_file(service_ptr_t<file> m_file, bool m_file_opened, const char* const filename, abort_callback* p_abort);

static int foo_read(void* user_data, uint8_t* dst, int64_t offset, int length) {
    foo_priv_t* priv = (foo_priv_t*)user_data;
    int read_total = 0;
    if (!priv->m_file_opened || !dst || length <= 0 || offset < 0)
        return 0;

    priv->offset = offset; /* current offset */

    /* is the part of the requested length in the buffer? */
    if (priv->offset >= priv->buf_offset && priv->offset < priv->buf_offset + priv->valid_size) {
        size_t buf_limit;
        int buf_into = (int)(priv->offset - priv->buf_offset);

        buf_limit = priv->valid_size - buf_into;
        if (buf_limit > length)
            buf_limit = length;

        memcpy(dst, priv->buf + buf_into, buf_limit);
        read_total += buf_limit;
        length -= buf_limit;
        priv->offset += buf_limit;
        dst += buf_limit;
    }


    /* read the rest of the requested length */
    while (length > 0) {
        size_t buf_limit;

        /* ignore requests at EOF */
        if (priv->offset >= priv->file_size) {
            //offset = sf->file_size; /* seems fseek doesn't clamp offset */
            //VGM_ASSERT_ONCE(offset > sf->file_size, "STDIO: reading over file_size 0x%x @ 0x%lx + 0x%x\n", sf->file_size, offset, length);
            break;
        }

        /* position to new offset */
        try {
            priv->m_file->seek(priv->offset, *priv->p_abort);
        } catch (...) {
            break; /* this shouldn't happen in our code */
        }

        /* fill the buffer (offset now is beyond buf_offset) */
        try {
            priv->buf_offset = priv->offset;
            priv->valid_size = priv->m_file->read(priv->buf, priv->buf_size, *priv->p_abort);
        } catch(...) {
            break; /* improbable? */
        }

        /* decide how much must be read this time */
        if (length > priv->buf_size)
            buf_limit = priv->buf_size;
        else
            buf_limit = length;

        /* give up on partial reads (EOF) */
        if (priv->valid_size < buf_limit) {
            memcpy(dst, priv->buf, priv->valid_size);
            priv->offset += priv->valid_size;
            read_total += priv->valid_size;
            break;
        }

        /* use the new buffer */
        memcpy(dst, priv->buf, buf_limit);
        priv->offset += buf_limit;
        read_total += buf_limit;
        length -= buf_limit;
        dst += buf_limit;
    }

    return read_total;
}

static int64_t foo_get_size(void* user_data) {
    foo_priv_t* priv = (foo_priv_t*)user_data;

    return priv->file_size;
}

static const char* foo_get_name(void* user_data) {
    foo_priv_t* priv = (foo_priv_t*)user_data;

    return priv->name;
}

static libstreamfile_t* foo_open(void* user_data, const char* const filename) {
    foo_priv_t* priv = (foo_priv_t*)user_data;

    if (!priv || !filename)
        return NULL;

    // vgmstream may need to open "files based on another" (like a changing extension) and "files in the same subdir" (like .txth)
    // or read "base filename" to do comparison. When dealing with archives (foo_unpack plugin) the later two cases would fail, since
    // vgmstream doesn't separate the  "|" special notation foo_unpack adds.
    // To fix this, when this SF is part of an archive we give vgmstream the name without | and restore the archive on open
    // - get name:      "unpack://zip|23|file://C:\file.zip|subfile.adpcm"
    // > returns:       "unpack://zip|23|file://C:\subfile.adpcm" (otherwise base name would be "file.zip|subfile.adpcm")
    // - try opening    "unpack://zip|23|file://C:\.txth
    // > opens:         "unpack://zip|23|file://C:\file.zip|.txth
    // (assumes archives won't need to open files outside archives, and goes before filedup trick)
    if (priv->archname) {
        char finalname[FOO_PATH_LIMIT];
        const char* filepart = NULL; 

        // newly open files should be "(current-path)\newfile" or "(current-path)\folder\newfile", so we need to make
        // (archive-path = current-path)\(rest = newfile plus new folders)

        int filename_len = strlen(filename);
        if (filename_len > priv->archpath_end) {
            filepart = &filename[priv->archpath_end];
        } else  {
            filepart = strrchr(filename, '\\'); // vgmstream shouldn't remove paths though
            if (!filepart)
                filepart = filename;
            else
                filepart += 1;
        }

        //TODO improve str ops

        int filepart_len = strlen(filepart);
        if (priv->archfile_end + filepart_len + 1 >= sizeof(finalname))
            return NULL;
        // copy current path+archive ("unpack://zip|23|file://C:\file.zip|")
        memcpy(finalname, priv->archname, priv->archfile_end);
        // concat possible extra dirs and filename ("unpack://zip|23|file://C:\file.zip|" + "folder/bgm01.vag")
        memcpy(finalname + priv->archfile_end, filepart, filepart_len);
        finalname[priv->archfile_end + filepart_len] = '\0';

        // normalize subfolders inside archives to use "/" (path\archive.ext|subfolder/file.ext)
        for (int i = priv->archfile_end; i < sizeof(finalname); i++) {
            if (finalname[i] == '\0')
                break;
            if (finalname[i] == '\\')
                finalname[i] = '/';
        }

        //console::formatter() << "finalname: " << finalname;
        return open_foo_streamfile_internal(finalname, priv->p_abort, NULL);
    }

    // if same name, duplicate the file pointer we already have open
    if (priv->m_file_opened && !strcmp(priv->name, filename)) {
        service_ptr_t<file> m_file = priv->m_file; //copy?
        libstreamfile_t* new_sf = open_foo_streamfile_from_file(m_file, priv->m_file_opened, filename, priv->p_abort);
        if (new_sf) {
            return new_sf;
        }
        // failure, close it and try the default path (which will probably fail a second time)
    }

    // a normal open, open a new file
    return open_foo_streamfile_internal(filename, priv->p_abort, NULL);
}

static void foo_close(libstreamfile_t* libsf) {
    if (!libsf)
        return;

    foo_priv_t* priv = (foo_priv_t*)libsf->user_data;
    if (priv) {
        priv->m_file.release(); //release alloc'ed ptr
        free(priv->name);
        free(priv->archname);
        free(priv->buf);
    }
    free(priv);
    free(libsf);
}


static libstreamfile_t* open_foo_streamfile_from_file(service_ptr_t<file> m_file, bool m_file_opened, const char* const filename, abort_callback* p_abort) {
    libstreamfile_t* libsf;
    const int buf_size = FOO_STREAMFILE_DEFAULT_BUFFER_SIZE;

    libsf = (libstreamfile_t*)calloc(1, sizeof(libstreamfile_t));
    if (!libsf) goto fail;

    libsf->read = (int (*)(void*, uint8_t*, int64_t, int)) foo_read;
    libsf->get_size = (int64_t (*)(void*)) foo_get_size;
    libsf->get_name = (const char* (*)(void*)) foo_get_name;
    libsf->open = (libstreamfile_t* (*)(void*, const char* const)) foo_open;
    libsf->close = (void (*)(libstreamfile_t*)) foo_close;

    libsf->user_data = (foo_priv_t*)calloc(1, sizeof(foo_priv_t));
    if (!libsf->user_data) goto fail;

    foo_priv_t* priv = (foo_priv_t*)libsf->user_data;
    priv->m_file_opened = m_file_opened;
    priv->m_file = m_file;
    priv->p_abort = p_abort;
    priv->buf_size = buf_size;
    priv->buf = (uint8_t*)calloc(buf_size, sizeof(uint8_t));
    if (!priv->buf) goto fail;

    //TODO: foobar filenames look like "file://C:\path\to\file.adx"
    // maybe should hide the internal protocol and restore on open?
    priv->name = strdup(filename);
    if (!priv->name)  goto fail;
    priv->name_len = strlen(priv->name);

    // foobar supports .zip/7z/etc archives directly, in this format: "unpack://zip|(number))|file://C:\path\to\file.zip|subfile.adx"
    // Detect if current is inside archive, so when trying to get filename or open companion files it's handled correctly    
    // Subfolders have inside the archive use / instead or / (path\archive.zip|subfolder/file)
    if (strncmp(filename, "unpack", 6) == 0) {
        const char* archfile_ptr = strrchr(priv->name, '|');
        if (archfile_ptr)
            priv->archfile_end = (int)((intptr_t)archfile_ptr + 1 - (intptr_t)priv->name); // after "|""

        const char* archpath_ptr = strrchr(priv->name, '\\');
        if (archpath_ptr)
            priv->archpath_end = (int)((intptr_t)archpath_ptr + 1 - (intptr_t)priv->name); // after "\\"

        if (priv->archpath_end <= 0 || priv->archfile_end <= 0 || priv->archpath_end > priv->archfile_end || 
                priv->archfile_end > priv->name_len || priv->archfile_end >= FOO_PATH_LIMIT) {
            // ???
            priv->archpath_end = 0;
            priv->archfile_end = 0;
        }
        else {
            priv->archname = strdup(filename);
            if (!priv->archname)  goto fail;
            priv->archname_len = priv->name_len;
            int copy_len = strlen(&priv->archname[priv->archfile_end]);

            // change from "(path)\\(archive)|(filename)" to "(path)\\filename)" (smaller so shouldn't overrun)
            memcpy(priv->name + priv->archpath_end, priv->archname + priv->archfile_end, copy_len);
            priv->name[priv->archpath_end + copy_len] = '\0';

            //;console::formatter() << "base name: " << sf->name;
        }
    }

    /* cache file_size */
    if (priv->m_file_opened)
        priv->file_size = priv->m_file->get_size(*priv->p_abort);
    else
        priv->file_size = 0;

    /* STDIO has an optimization to close unneeded FDs if file size is less than buffer,
     * but seems foobar doesn't need this (reuses FDs?) */

    return libsf;
fail:
    foo_close(libsf);
    return NULL;
}

static libstreamfile_t* open_foo_streamfile_internal(const char* const filename, abort_callback* p_abort, t_filestats* stats) {
    service_ptr_t<file> infile;

    try {
        bool infile_exists = filesystem::g_exists(filename, *p_abort);
        if (!infile_exists) {
            /* allow non-existing files in some cases */
            if (!libvgmstream_is_virtual_filename(filename))
                return NULL;
        }

        if (infile_exists) {
            filesystem::g_open_read(infile, filename, *p_abort);
            if(stats) *stats = infile->get_stats(*p_abort);
        }
        
        libstreamfile_t* libsf = open_foo_streamfile_from_file(infile, infile_exists, filename, p_abort);
        if (!libsf) {
            //m_file.release(); //refcounted and cleaned after it goes out of scope
        }
        return libsf;

    } catch (...) {
        /* somehow foobar2000 throws an exception on g_exists when filename has a double \
         * (traditionally Windows treats that like a single slash and fopen handles it fine) */
        return NULL;
    }
}

libstreamfile_t* open_foo_streamfile(const char* const filename, abort_callback* p_abort, t_filestats* stats) {
    return open_foo_streamfile_internal(filename, p_abort, stats);
}
