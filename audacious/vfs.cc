#include <glib.h>
#include <cstdlib>

#include <libaudcore/plugin.h>

#include "../src/vgmstream.h"
#include "plugin.h"
#include "vfs.h"

typedef struct _VFSSTREAMFILE {
  STREAMFILE sf;
  VFSFile *vfsFile;
  off_t offset;
  char name[32768];
  //char realname[32768];
} VFSSTREAMFILE;

static STREAMFILE *open_vfs_by_VFSFILE(VFSFile *file, const char *path);

static size_t read_vfs(VFSSTREAMFILE *streamfile, uint8_t *dest, off_t offset,
                       size_t length) {
  size_t sz;
  // if the offsets don't match, then we need to perform a seek
  if (streamfile->offset != offset) {
    streamfile->vfsFile->fseek(offset, VFS_SEEK_SET);
    streamfile->offset = offset;
  }

  sz = streamfile->vfsFile->fread(dest, 1, length);
  // increment our current offset
  streamfile->offset += sz;

  return sz;
}

static void close_vfs(VFSSTREAMFILE *streamfile) {
  debugMessage("close_vfs");
  free(streamfile->vfsFile);
  free(streamfile);
}

static size_t get_size_vfs(VFSSTREAMFILE *streamfile) {
  return streamfile->vfsFile->fsize();
}

static size_t get_offset_vfs(VFSSTREAMFILE *streamfile) {
  return streamfile->offset;
}

static void get_name_vfs(VFSSTREAMFILE *streamfile, char *buffer,
                         size_t length) {
  strncpy(buffer, streamfile->name, length);
  buffer[length - 1] = '\0';
}

static STREAMFILE *open_vfs_impl(VFSSTREAMFILE *streamfile,
                                 const char *const filename,
                                 size_t buffersize) {
  if (!filename)
    return NULL;

  return open_vfs(filename);
}

STREAMFILE *open_vfs_by_VFSFILE(VFSFile *file, const char *path) {
  VFSSTREAMFILE *streamfile = (VFSSTREAMFILE *)malloc(sizeof(VFSSTREAMFILE));
  if (!streamfile)
    return NULL;

  // success, set our pointers
  memset(streamfile, 0, sizeof(VFSSTREAMFILE));

  streamfile->sf.read = (size_t (*)(STREAMFILE *, uint8_t *, off_t, size_t))read_vfs;
  streamfile->sf.get_size = (size_t (*)(STREAMFILE *))get_size_vfs;
  streamfile->sf.get_offset = (off_t (*)(STREAMFILE *))get_offset_vfs;
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
  //  gchar *realname = g_filename_from_uri(path, NULL, NULL);
  //  strncpy(streamfile->realname, realname, sizeof(streamfile->realname));
  //  streamfile->realname[sizeof(streamfile->realname) - 1] = '\0';
  //  g_free(realname);
  //}

  return &streamfile->sf;
}

STREAMFILE *open_vfs(const char *path) {
  VFSFile *vfsFile = new VFSFile(path, "rb");
  if (!vfsFile || !*vfsFile) {
    delete vfsFile;
    return NULL;
  }

  return open_vfs_by_VFSFILE(vfsFile, path);
}
