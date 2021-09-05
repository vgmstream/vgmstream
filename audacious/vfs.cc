#include <glib.h>
#include <cstdlib>

#include <libaudcore/plugin.h>

extern "C" {
#include "../src/vgmstream.h"
}
#include "plugin.h"
#include "vfs.h"

typedef struct {
    STREAMFILE sf;
    VFSFile *vfsFile;
    offv_t offset;
    char name[32768];
} VFS_STREAMFILE;

static STREAMFILE *open_vfs_by_VFSFILE(VFSFile *file, const char *path);

static size_t read_vfs(VFS_STREAMFILE *streamfile, uint8_t *dest, offv_t offset, size_t length) {
    size_t bytes_read;

    if (/*!streamfile->vfsFile ||*/ !dest || length <= 0 || offset < 0)
        return 0;

    // if the offsets don't match, then we need to perform a seek
    if (streamfile->offset != offset) {
        int ok = streamfile->vfsFile->fseek(offset, VFS_SEEK_SET);
        if (ok != 0) return 0;
        streamfile->offset = offset;
    }

    bytes_read = streamfile->vfsFile->fread(dest, 1, length);
    streamfile->offset += bytes_read;

    return bytes_read;
}

static void close_vfs(VFS_STREAMFILE *streamfile) {
    //if (streamfile->vfsFile)
    delete streamfile->vfsFile; //fcloses the internal file too
    free(streamfile);
}

static size_t get_size_vfs(VFS_STREAMFILE *streamfile) {
    //if (!streamfile->vfsFile)
    //    return 0;
    return streamfile->vfsFile->fsize();
}

static size_t get_offset_vfs(VFS_STREAMFILE *streamfile) {
    return streamfile->offset;
}

static void get_name_vfs(VFS_STREAMFILE *streamfile, char *buffer, size_t length) {
    strncpy(buffer, streamfile->name, length);
    buffer[length - 1] = '\0';
}

static STREAMFILE *open_vfs_impl(VFS_STREAMFILE *streamfile, const char *const filename, size_t buffersize) {
    if (!filename)
        return NULL;

    return open_vfs(filename);
}

STREAMFILE *open_vfs_by_VFSFILE(VFSFile *file, const char *path) {
    VFS_STREAMFILE *streamfile = (VFS_STREAMFILE *)malloc(sizeof(VFS_STREAMFILE));
    if (!streamfile)
        return NULL;

    // success, set our pointers
    memset(streamfile, 0, sizeof(VFS_STREAMFILE));

    streamfile->sf.read = (size_t (*)(STREAMFILE *, uint8_t *, offv_t, size_t))read_vfs;
    streamfile->sf.get_size = (size_t (*)(STREAMFILE *))get_size_vfs;
    streamfile->sf.get_offset = (offv_t (*)(STREAMFILE *))get_offset_vfs;
    streamfile->sf.get_name = (void (*)(STREAMFILE *, char *, size_t))get_name_vfs;
    streamfile->sf.open = (STREAMFILE *(*)(STREAMFILE *, const char *, size_t))open_vfs_impl;
    streamfile->sf.close = (void (*)(STREAMFILE *))close_vfs;

    streamfile->vfsFile = file;
    streamfile->offset = 0;
    strncpy(streamfile->name, path, sizeof(streamfile->name));
    streamfile->name[sizeof(streamfile->name) - 1] = '\0';

    // for reference, actual file path ("name" has protocol path, file://...).
    // name should work for all situations but in case it's needed again maybe
    // get_name should always return realname, as it's used to open companion VFSFiles
    //{
    //    gchar *realname = g_filename_from_uri(path, NULL, NULL);
    //    strncpy(streamfile->realname, realname, sizeof(streamfile->realname));
    //    streamfile->realname[sizeof(streamfile->realname) - 1] = '\0';
    //    g_free(realname);
    //}

    return &streamfile->sf;
}

STREAMFILE *open_vfs(const char *path) {
    VFSFile *vfsFile = new VFSFile(path, "rb");
    if (!vfsFile || !*vfsFile) {
        delete vfsFile;
        return NULL;
    }

#if 0 // files that don't exist seem blocked by probe.cc before reaching here
    bool infile_exists = vfsFile && *vfsFile;
    if (!infile_exists) {
        /* allow non-existing files in some cases */
        if (!vgmstream_is_virtual_filename(path)) {
            delete vfsFile;
            return NULL;
        }
        vfsFile = NULL;
    }
#endif
    return open_vfs_by_VFSFILE(vfsFile, path);
}
