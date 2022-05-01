#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <io.h>

#include <foobar2000/SDK/foobar2000.h>

extern "C" {
#include "../src/streamfile.h"
//#include "../src/vgmstream.h"
#include "../src/util.h"
}
#include "foo_vgmstream.h"


/* a STREAMFILE that operates via foobar's file service using a buffer */
typedef struct {
    STREAMFILE vt;              /* callbacks */

    bool m_file_opened;         /* if foobar IO service opened the file */
    service_ptr_t<file> m_file; /* foobar IO service */
    abort_callback* p_abort;    /* foobar error stuff */
    /*const*/ char* name;       /* IO filename */
    int name_len;               /* cache */

    char* archname;             /* for foobar's foo_unpack archives */
    int archname_len;           /* cache */
    int archpath_end;           /* where the last \ ends before archive name */
    int archfile_end;           /* where the last | ends before file name */

    offv_t offset;              /* last read offset (info) */
    offv_t buf_offset;          /* current buffer data start */
    uint8_t* buf;               /* data buffer */
    size_t buf_size;            /* max buffer size */
    size_t valid_size;          /* current buffer size */
    size_t file_size;           /* buffered file size */
} FOO_STREAMFILE;

static STREAMFILE* open_foo_streamfile_buffer(const char* const filename, size_t buf_size, abort_callback* p_abort, t_filestats* stats);
static STREAMFILE* open_foo_streamfile_buffer_by_file(service_ptr_t<file> m_file, bool m_file_opened, const char* const filename, size_t buf_size, abort_callback* p_abort);

static size_t foo_read(FOO_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    size_t read_total = 0;

    if (!sf || !sf->m_file_opened || !dst || length <= 0 || offset < 0)
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


    /* read the rest of the requested length */
    while (length > 0) {
        size_t buf_limit;

        /* ignore requests at EOF */
        if (offset >= sf->file_size) {
            //offset = sf->file_size; /* seems fseek doesn't clamp offset */
            //VGM_ASSERT_ONCE(offset > sf->file_size, "STDIO: reading over file_size 0x%x @ 0x%lx + 0x%x\n", sf->file_size, offset, length);
            break;
        }

        /* position to new offset */
        try {
            sf->m_file->seek(offset, *sf->p_abort);
        } catch (...) {
            break; /* this shouldn't happen in our code */
        }

        /* fill the buffer (offset now is beyond buf_offset) */
        try {
            sf->buf_offset = offset;
            sf->valid_size = sf->m_file->read(sf->buf, sf->buf_size, *sf->p_abort);
        } catch(...) {
            break; /* improbable? */
        }

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
static size_t foo_get_size(FOO_STREAMFILE* sf) {
    return sf->file_size;
}
static offv_t foo_get_offset(FOO_STREAMFILE* sf) {
    return sf->offset;
}
static void foo_get_name(FOO_STREAMFILE* sf, char* name, size_t name_size) {
    int copy_size = sf->name_len + 1;
    if (copy_size > name_size)
        copy_size = name_size;

    memcpy(name, sf->name, copy_size);
    name[copy_size - 1] = '\0';

    /*
    //TODO: check again (looks like a truncate-from-the-end copy, probably a useless remnant of olden times)
    if (sf->name_len > name_size) {
        strcpy(name, sf->name + sf->name_len - name_size + 1);
    } else {
        strcpy(name, sf->name);
    }
    */
}
static void foo_close(FOO_STREAMFILE* sf) {
    sf->m_file.release(); //release alloc'ed ptr
    free(sf->name);
    free(sf->archname);
    free(sf->buf);
    free(sf);
}

static STREAMFILE* foo_open(FOO_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    service_ptr_t<file> m_file;

    if (!filename)
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
    if (sf->archname) {
        char finalname[PATH_LIMIT];
        const char* dirsep = NULL; 

        // newly open files should be "(current-path)\newfile" or "(current-path)\folder\newfile", so we need to make
        // (archive-path = current-path)\(rest = newfile plus new folders)
        int filename_len = strlen(filename);

        if (filename_len > sf->archpath_end) {
            dirsep = &filename[sf->archpath_end];
        } else  {
            dirsep = strrchr(filename, '\\'); // vgmstream shouldn't remove paths though
            if (!dirsep)
                dirsep = filename;
            else
                dirsep += 1;
        }

        //TODO improve strops
        memcpy(finalname, sf->archname, sf->archfile_end); //copy current path+archive
        finalname[sf->archfile_end] = '\0';
        concatn(sizeof(finalname), finalname, dirsep); //paste possible extra dirs and filename

        // subfolders inside archives use "/" (path\archive.ext|subfolder/file.ext)
        for (int i = sf->archfile_end; i < sizeof(finalname); i++) {
            if (finalname[i] == '\0')
                break;
            if (finalname[i] == '\\')
                finalname[i] = '/';
        }

        //console::formatter() << "finalname: " << finalname;
        return open_foo_streamfile_buffer(finalname, buf_size, sf->p_abort, NULL);
    }

    // if same name, duplicate the file pointer we already have open
    if (sf->m_file_opened && !strcmp(sf->name, filename)) {
        m_file = sf->m_file; //copy?
        {
            STREAMFILE* new_sf = open_foo_streamfile_buffer_by_file(m_file, sf->m_file_opened, filename, buf_size, sf->p_abort);
            if (new_sf) {
                return new_sf;
            }
            // failure, close it and try the default path (which will probably fail a second time)
        }
    }

    // a normal open, open a new file
    return open_foo_streamfile_buffer(filename, buf_size, sf->p_abort, NULL);
}

static STREAMFILE* open_foo_streamfile_buffer_by_file(service_ptr_t<file> m_file, bool m_file_opened, const char* const filename, size_t buf_size, abort_callback* p_abort) {
    uint8_t* buf;
    FOO_STREAMFILE* this_sf;

    buf = (uint8_t*) calloc(buf_size, sizeof(uint8_t));
    if (!buf) goto fail;

    this_sf = (FOO_STREAMFILE*) calloc(1, sizeof(FOO_STREAMFILE));
    if (!this_sf) goto fail;

    this_sf->vt.read = (size_t (__cdecl *)(_STREAMFILE*, uint8_t*, offv_t, size_t)) foo_read;
    this_sf->vt.get_size = (size_t (__cdecl *)(_STREAMFILE*)) foo_get_size;
    this_sf->vt.get_offset = (offv_t (__cdecl *)(_STREAMFILE*)) foo_get_offset;
    this_sf->vt.get_name = (void (__cdecl *)(_STREAMFILE*, char*, size_t)) foo_get_name;
    this_sf->vt.open = (_STREAMFILE* (__cdecl *)(_STREAMFILE* ,const char* const, size_t)) foo_open;
    this_sf->vt.close = (void (__cdecl *)(_STREAMFILE* )) foo_close;

    this_sf->m_file_opened = m_file_opened;
    this_sf->m_file = m_file;
    this_sf->p_abort = p_abort;
    this_sf->buf_size = buf_size;
    this_sf->buf = buf;

    //TODO: foobar filenames look like "file://C:\path\to\file.adx"
    // maybe should hide the internal protocol and restore on open?
    this_sf->name = strdup(filename);
    if (!this_sf->name)  goto fail;
    this_sf->name_len = strlen(this_sf->name);

    // foobar supports .zip/7z/etc archives directly, in this format: "unpack://zip|(number))|file://C:\path\to\file.zip|subfile.adx"
    // Detect if current is inside archive, so when trying to get filename or open companion files it's handled correctly    
    // Subfolders have inside the archive use / instead or / (path\archive.zip|subfolder/file)
    if (strncmp(filename, "unpack", 6) == 0) {
        const char* archfile_ptr = strrchr(this_sf->name, '|');
        if (archfile_ptr)
            this_sf->archfile_end = (intptr_t)archfile_ptr + 1 - (intptr_t)this_sf->name; // after "|""

        const char* archpath_ptr = strrchr(this_sf->name, '\\');
        if (archpath_ptr)
            this_sf->archpath_end = (intptr_t)archpath_ptr + 1 - (intptr_t)this_sf->name; // after "\\"

        if (this_sf->archpath_end <= 0 || this_sf->archfile_end <= 0 || this_sf->archpath_end > this_sf->archfile_end || 
                this_sf->archfile_end > this_sf->name_len || this_sf->archfile_end >= PATH_LIMIT) {
            // ???
            this_sf->archpath_end = 0;
            this_sf->archfile_end = 0;
        }
        else {
            this_sf->archname = strdup(filename);
            if (!this_sf->archname)  goto fail;
            this_sf->archname_len = this_sf->name_len;

            // change from "(path)\\(archive)|(filename)" to "(path)\\filename)"
            this_sf->name[this_sf->archpath_end] = '\0';
            concatn(this_sf->name_len, this_sf->name, &this_sf->archname[this_sf->archfile_end]);
        }
    }

    /* cache file_size */
    if (this_sf->m_file_opened)
        this_sf->file_size = this_sf->m_file->get_size(*this_sf->p_abort);
    else
        this_sf->file_size = 0;

    /* STDIO has an optimization to close unneeded FDs if file size is less than buffer,
     * but seems foobar doesn't need this (reuses FDs?) */

    return &this_sf->vt;

fail:
    free(buf);
    free(this_sf);
    return NULL;
}

static STREAMFILE* open_foo_streamfile_buffer(const char* const filename, size_t buf_size, abort_callback* p_abort, t_filestats* stats) {
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
        
        sf = open_foo_streamfile_buffer_by_file(infile, infile_exists, filename, buf_size, p_abort);
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
