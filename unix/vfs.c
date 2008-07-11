#include <audacious/util.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>
#include <audacious/strings.h>
#include <glib.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include "version.h"
#include "../src/vgmstream.h"
#include "vfs.h"

/* vfs_dup doesn't actually work, it just returns us the same pointer
   as the one we pass in.  Therefore, the offset optimization doesn't work.
   So by not defining SUPPORT_DUP, we end up with two VFS handles with
   unique offsets.
*/ 
//#define SUPPORT_DUP 1

typedef struct _VFSSTREAMFILE
{
  STREAMFILE sf;
  VFSFile *vfsFile;
  off_t offset;
  char name[260];
  char realname[260];
} VFSSTREAMFILE;

static size_t read_vfs(VFSSTREAMFILE *streamfile,uint8_t *dest,off_t offset,size_t length)
{
  size_t sz;
  /* if the offsets don't match, then we need to perform a seek */
  if (streamfile->offset != offset) 
  {
    aud_vfs_fseek(streamfile->vfsFile,offset,SEEK_SET);
    streamfile->offset = offset;
  }
  
  sz = aud_vfs_fread(dest,1,length,streamfile->vfsFile);
  /* increment our current offset */
  if (sz >= 0)
    streamfile->offset += sz;
  
  return sz;
}

static void close_vfs(VFSSTREAMFILE *streamfile)
{
  aud_vfs_fclose(streamfile->vfsFile);
  free(streamfile);
}

static size_t get_size_vfs(VFSSTREAMFILE *streamfile)
{
  return aud_vfs_fsize(streamfile->vfsFile);
}

static size_t get_offset_vfs(VFSSTREAMFILE *streamfile)
{
  //return aud_vfs_ftell(streamfile->vfsFile);
  return streamfile->offset;
}

static void get_name_vfs(VFSSTREAMFILE *streamfile,char *buffer,size_t length)
{
  strcpy(buffer,streamfile->name);
}

static void get_realname_vfs(VFSSTREAMFILE *streamfile,char *buffer,size_t length)
{
    strcpy(buffer,streamfile->realname);
}

static STREAMFILE *open_vfs_by_VFSFILE(VFSFile *file,const char *path);

static STREAMFILE *open_vfs_impl(VFSSTREAMFILE *streamfile,const char * const filename,size_t buffersize) 
{
#ifdef SUPPORT_DUP
  VFSFile *newfile;
  STREAMFILE *newstreamFile;
#endif
  if (!filename)
    return NULL;
  // if same name, duplicate the file pointer we already have open
#ifdef SUPPORT_DUP
  if (!strcmp(streamfile->name,filename)) {
    if ((newfile = aud_vfs_dup(streamfile->vfsFile)))
    {
      newstreamFile = open_vfs_by_VFSFILE(newfile,filename);
      if (newstreamFile) { 
	return newstreamFile;
      }
      // failure, close it and try the default path (which will probably fail a second time)
      aud_vfs_fclose(newfile);
    }
  }
#endif
  return open_vfs(filename);
}

static STREAMFILE *open_vfs_by_VFSFILE(VFSFile *file,const char *path)
{
  VFSSTREAMFILE *streamfile = malloc(sizeof(VFSSTREAMFILE));
  if (!streamfile)
    return NULL;
  
  /* success, set our pointers */
  memset(streamfile,0,sizeof(VFSSTREAMFILE));

  streamfile->sf.read = (void*)read_vfs;
  streamfile->sf.get_size = (void*)get_size_vfs;
  streamfile->sf.get_offset = (void*)get_offset_vfs;
  streamfile->sf.get_name = (void*)get_name_vfs;
  streamfile->sf.get_realname = (void*)get_realname_vfs;
  streamfile->sf.open = (void*)open_vfs_impl;
  streamfile->sf.close = (void*)close_vfs;

  streamfile->vfsFile = file;
  streamfile->offset = 0;
  strcpy(streamfile->name,path);
  {
      gchar* realname = g_filename_from_uri(path,NULL,NULL);
      strcpy(streamfile->realname,realname);
      g_free(realname);
  }
  
  return &streamfile->sf;
}

STREAMFILE *open_vfs(const char *path)
{
  VFSFile *vfsFile = aud_vfs_fopen(path,"rb");
  if (!vfsFile)
    return NULL;

  return open_vfs_by_VFSFILE(vfsFile,path);
}
