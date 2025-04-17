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
    GUri* uri;
    gchar* filename;
    gchar* hostname;
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
    if (priv->filename)
        return priv->filename;

    return g_uri_get_path(priv->uri);
}

static libstreamfile_t* vfs_open(void* user_data, const char* filename) {
    vfs_priv_t* priv = (vfs_priv_t*)user_data;

    if (!filename)
        return NULL;

    // Reconstruct a URI based on which name vfs_get_name() returns.
    gchar* new_uri = NULL;
    if (priv->filename)
        new_uri = g_filename_to_uri(filename, priv->hostname, NULL);
    if (!new_uri) {
        // Replace the path and drop the query and fragment.
        new_uri = g_uri_join(
            G_URI_FLAGS_NONE,
            g_uri_get_scheme(priv->uri),
            g_uri_get_userinfo(priv->uri),
            g_uri_get_host(priv->uri),
            g_uri_get_port(priv->uri),
            filename,
            NULL,
            NULL
        );
    }

    if (!new_uri)
        return NULL;

    libstreamfile_t* result = open_vfs(new_uri);
    g_free(new_uri);
    return result;
}

static void vfs_close(libstreamfile_t* libsf) {
    if (libsf->user_data) {
        vfs_priv_t* priv = (vfs_priv_t*)libsf->user_data;
        g_free(priv->hostname);
        g_free(priv->filename);
        g_uri_unref(priv->uri);
        delete priv->vfsFile; //fcloses the internal file too
        free(priv);
    }
    free(libsf);
}

static libstreamfile_t* open_vfs_by_vfsfile(VFSFile* file, const char* path) {

    vfs_priv_t* priv = NULL;
    libstreamfile_t* libsf = (libstreamfile_t*)calloc(1, sizeof(libstreamfile_t));
    if (!libsf) return NULL;

    libsf->read = vfs_read;
    libsf->get_size = vfs_get_size;
    libsf->get_name = vfs_get_name;
    libsf->open = vfs_open;
    libsf->close = vfs_close;

    libsf->user_data = (vfs_priv_t*)calloc(1, sizeof(vfs_priv_t));
    if (!libsf->user_data) goto fail;

    priv = (vfs_priv_t*)libsf->user_data;
    priv->vfsFile = file;
    priv->offset = 0;

    // path is a URI, not a filesystem path.
    // Characters such as # are percent-encoded.
    // G_URI_FLAGS_NONE will decode percent-encoded characters
    // in all parts of the URI.
    priv->uri = g_uri_parse(path, G_URI_FLAGS_NONE, NULL);
    if (!priv->uri) goto fail;

    // From <https://docs.gtk.org/glib/struct.Uri.html#file-uris>:
    //
    // > Note that Windows and Unix both define special rules for parsing file:// URIs
    // > (involving non-UTF-8 character sets on Unix,
    // > and the interpretation of path separators on Windows).
    // > GUri does not implement these rules.
    // > Use g_filename_from_uri() and g_filename_to_uri()
    // > if you want to properly convert between file:// URIs and local filenames.
    //
    // Since vgmstream normally expects filesystem paths (filenames),
    // let's give it a filesystem path if we can.
    //
    // g_filename_from_uri will return NULL if the URI is not a file:// URI.
    // In that case, we'll fall back to using the GUri* instead.
    priv->filename = g_filename_from_uri(path, &priv->hostname, NULL);

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
