#include <glib.h>
#include <cstdlib>
#include <libaudcore/plugin.h>

extern "C" {
#include "../src/libvgmstream.h"
}
#include "plugin.h"
#include "vfs.h"

typedef struct {
    VFSFile* vfsFile;
    int64_t offset;
    char name[0x4000];
} vfs_priv_t;

static libstreamfile_t* open_vfs_by_vfsfile(VFSFile* file, const char* path);

static int vfs_read(void* user_data, uint8_t* dst, int64_t offset, int length) {
    vfs_priv_t* priv = (vfs_priv_t*)user_data;
    if (/*!priv->vfsFile ||*/ !dst || length <= 0 || offset < 0)
        return 0;

    // if the offsets don't match, then we need to perform a seek
    if (priv->offset != offset) {
        int ok = priv->vfsFile->fseek(offset, VFS_SEEK_SET);
        if (ok != 0) return 0;
        priv->offset = offset;
    }

    size_t bytes_read = priv->vfsFile->fread(dst, sizeof(char), length);
    priv->offset += bytes_read;

    return bytes_read;
}

static int64_t vfs_get_size(void* user_data) {
    vfs_priv_t* priv = (vfs_priv_t*)user_data;
    //if (!priv->vfsFile)
    //    return 0;
    return priv->vfsFile->fsize();
}

static const char* vfs_get_name(void* user_data) {
    vfs_priv_t* priv = (vfs_priv_t*)user_data;
    return priv->name;
}

static libstreamfile_t* vfs_open(void* user_data, const char* filename) {
    if (!filename)
        return NULL;

    return open_vfs(filename);
}

static void vfs_close(libstreamfile_t* libsf) {
    if (libsf->user_data) {
        vfs_priv_t* priv = (vfs_priv_t*)libsf->user_data;
        //if (streamfile->vfsFile)
        delete priv->vfsFile; //fcloses the internal file too
    }
    free(libsf);
}

static libstreamfile_t* open_vfs_by_vfsfile(VFSFile* file, const char* path) {

    vfs_priv_t* priv = NULL;
    libstreamfile_t* libsf = (libstreamfile_t*)calloc(1, sizeof(libstreamfile_t));
    if (!libsf) return NULL;

    libsf->read = (int (*)(void*, uint8_t*, int64_t, int)) vfs_read;
    libsf->get_size = (int64_t (*)(void*)) vfs_get_size;
    libsf->get_name = (const char* (*)(void*)) vfs_get_name;
    libsf->open = (libstreamfile_t* (*)(void*, const char* const)) vfs_open;
    libsf->close = (void (*)(libstreamfile_t*)) vfs_close;

    libsf->user_data = (vfs_priv_t*)calloc(1, sizeof(vfs_priv_t));
    if (!libsf->user_data) goto fail;

    priv = (vfs_priv_t*)libsf->user_data;
    priv->vfsFile = file;
    priv->offset = 0;
    strncpy(priv->name, path, sizeof(priv->name));
    priv->name[sizeof(priv->name) - 1] = '\0';

    // for reference, actual file path ("name" has protocol path, file://...).
    // name should work for all situations but in case it's needed again maybe
    // get_name should always return realname, as it's used to open companion VFSFiles
    //{
    //    gchar *realname = g_filename_from_uri(path, NULL, NULL);
    //    strncpy(priv->realname, realname, sizeof(priv->realname));
    //    priv->realname[sizeof(priv->realname) - 1] = '\0';
    //    g_free(realname);
    //}

    return libsf;
fail:
    vfs_close(libsf);
    return NULL;
}

libstreamfile_t* open_vfs(const char *path) {
    VFSFile* vfsFile = new VFSFile(path, "rb");
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
    return open_vfs_by_vfsfile(vfsFile, path);
}
