#ifndef _AAX_STREAMFILE_H_
#define _AAX_STREAMFILE_H_
#include "../streamfile.h"

/* a streamfile representing a subfile inside another */

typedef struct _AAXSTREAMFILE {
    STREAMFILE sf;
    STREAMFILE *real_file;
    off_t start_physical_offset;
    size_t file_size;
} AAXSTREAMFILE;

static size_t read_aax(AAXSTREAMFILE *streamfile,uint8_t *dest,off_t offset,size_t length) {
    /* truncate at end of logical file */
    if (offset+length > streamfile->file_size) {
        long signed_length = length;
        signed_length = streamfile->file_size - offset;
        if (signed_length < 0) signed_length = 0;
        length = signed_length;
    }
    return read_streamfile(dest, streamfile->start_physical_offset+offset, length,streamfile->real_file);
}

static void close_aax(AAXSTREAMFILE *streamfile) {
    free(streamfile);
    return;
}

static size_t get_size_aax(AAXSTREAMFILE *streamfile) {
    return 0;
}

static size_t get_offset_aax(AAXSTREAMFILE *streamfile) {
    long offset = streamfile->real_file->get_offset(streamfile->real_file);
    offset -= streamfile->start_physical_offset;
    if (offset < 0) offset = 0;
    if (offset > streamfile->file_size) offset = streamfile->file_size;

    return offset;
}

static void get_name_aax(AAXSTREAMFILE *streamfile,char *buffer,size_t length) {
    strncpy(buffer,"ARBITRARY.ADX",length);
    buffer[length-1]='\0';
}

static STREAMFILE *open_aax_impl(AAXSTREAMFILE *streamfile,const char * const filename,size_t buffersize) {
    AAXSTREAMFILE *newfile;
    if (strcmp(filename,"ARBITRARY.ADX"))
        return NULL;

    newfile = malloc(sizeof(AAXSTREAMFILE));
    if (!newfile)
        return NULL;
    memcpy(newfile,streamfile,sizeof(AAXSTREAMFILE));
    return &newfile->sf;
}

static STREAMFILE *open_aax_with_STREAMFILE(STREAMFILE *file,off_t start_offset,size_t file_size) {
    AAXSTREAMFILE *streamfile = malloc(sizeof(AAXSTREAMFILE));

    if (!streamfile)
        return NULL;

    /* success, set our pointers */

    streamfile->sf.read = (void*)read_aax;
    streamfile->sf.get_size = (void*)get_size_aax;
    streamfile->sf.get_offset = (void*)get_offset_aax;
    streamfile->sf.get_name = (void*)get_name_aax;
    streamfile->sf.get_realname = (void*)get_name_aax;
    streamfile->sf.open = (void*)open_aax_impl;
    streamfile->sf.close = (void*)close_aax;
#ifdef PROFILE_STREAMFILE
    streamfile->sf.get_bytes_read = NULL;
    streamfile->sf.get_error_count = NULL;
#endif

    streamfile->real_file = file;
    streamfile->start_physical_offset = start_offset;
    streamfile->file_size = file_size;

    return &streamfile->sf;
}

#endif /* _AAX_STREAMFILE_H_ */
