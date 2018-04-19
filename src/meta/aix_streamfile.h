#ifndef _AIX_STREAMFILE_H_
#define _AIX_STREAMFILE_H_
#include "../streamfile.h"

/* a streamfile representing a subfile inside another, in blocked AIX format */

typedef struct _AIXSTREAMFILE {
    STREAMFILE sf;
    STREAMFILE *real_file;
    off_t start_physical_offset;
    off_t current_physical_offset;
    off_t current_logical_offset;
    off_t current_block_size;
    int stream_id;
} AIXSTREAMFILE;


/*static*/ STREAMFILE *open_aix_with_STREAMFILE(STREAMFILE *file, off_t start_offset, int stream_id);


static size_t read_aix(AIXSTREAMFILE *streamfile,uint8_t *dest,off_t offset,size_t length) {
    size_t sz = 0;

    /*printf("trying to read %x bytes from %x (str%d)\n",length,offset,streamfile->stream_id);*/
    while (length > 0) {
        int read_something = 0;

        /* read the beginning of the requested block, if we can */
        if (offset >= streamfile->current_logical_offset) {
            off_t to_read;
            off_t length_available;

            length_available = (streamfile->current_logical_offset + streamfile->current_block_size) - offset;

            if (length < length_available) {
                to_read = length;
            }
            else {
                to_read = length_available;
            }

            if (to_read > 0) {
                size_t bytes_read;

                bytes_read = read_streamfile(dest,
                        streamfile->current_physical_offset+0x10 + (offset-streamfile->current_logical_offset),
                        to_read,streamfile->real_file);

                sz += bytes_read;
                if (bytes_read != to_read) {
                    return sz; /* an error which we will not attempt to handle here */
                }

                read_something = 1;

                dest += bytes_read;
                offset += bytes_read;
                length -= bytes_read;
            }
        }

        if (!read_something) {
            /* couldn't read anything, must seek */
            int found_block = 0;

            /* as we have no memory we must start seeking from the beginning */
            if (offset < streamfile->current_logical_offset) {
                streamfile->current_logical_offset = 0;
                streamfile->current_block_size = 0;
                streamfile->current_physical_offset = streamfile->start_physical_offset;
            }

            /* seek ye forwards */
            while (!found_block) {
                /*printf("seek looks at %x\n",streamfile->current_physical_offset);*/
                switch (read_32bitBE(streamfile->current_physical_offset, streamfile->real_file)) {
                      case 0x41495850:  /* AIXP */
                          if (read_8bit(streamfile->current_physical_offset+8, streamfile->real_file) == streamfile->stream_id) {
                              streamfile->current_block_size = (uint16_t)read_16bitBE(streamfile->current_physical_offset+0x0a, streamfile->real_file);

                              if (offset >= streamfile->current_logical_offset+ streamfile->current_block_size) {
                                  streamfile->current_logical_offset += streamfile->current_block_size;
                              }
                              else {
                                  found_block = 1;
                              }
                          }

                          if (!found_block) {
                              streamfile->current_physical_offset += read_32bitBE(streamfile->current_physical_offset+0x04, streamfile->real_file) + 8;
                          }

                          break;
                      case 0x41495846:  /* AIXF */
                          /* shouldn't ever see this */
                      case 0x41495845:  /* AIXE */
                          /* shouldn't have reached the end o' the line... */
                      default:
                          return sz;
                          break;
                } /* end block/chunk type select */
            } /* end while !found_block */
        } /* end if !read_something */
    } /* end while length > 0 */

    return sz;
}

static void close_aix(AIXSTREAMFILE *streamfile) {
    free(streamfile);
    return;
}

static size_t get_size_aix(AIXSTREAMFILE *streamfile) {
    return 0;
}

static size_t get_offset_aix(AIXSTREAMFILE *streamfile) {
    return streamfile->current_logical_offset;
}

static void get_name_aix(AIXSTREAMFILE *streamfile,char *buffer,size_t length) {
    strncpy(buffer,"ARBITRARY.ADX",length);
    buffer[length-1]='\0';
}

static STREAMFILE *open_aix_impl(AIXSTREAMFILE *streamfile,const char * const filename,size_t buffersize) {
    AIXSTREAMFILE *newfile;
    if (strcmp(filename,"ARBITRARY.ADX"))
        return  NULL;

    newfile = malloc(sizeof(AIXSTREAMFILE));
    if (!newfile)
        return NULL;
    memcpy(newfile,streamfile,sizeof(AIXSTREAMFILE));
    return &newfile->sf;
}

/*static*/ STREAMFILE *open_aix_with_STREAMFILE(STREAMFILE *file, off_t start_offset, int stream_id) {
    AIXSTREAMFILE *streamfile = malloc(sizeof(AIXSTREAMFILE));

    if (!streamfile)
        return NULL;

    /* success, set our pointers */

    streamfile->sf.read = (void*)read_aix;
    streamfile->sf.get_size = (void*)get_size_aix;
    streamfile->sf.get_offset = (void*)get_offset_aix;
    streamfile->sf.get_name = (void*)get_name_aix;
    streamfile->sf.open = (void*)open_aix_impl;
    streamfile->sf.close = (void*)close_aix;

    streamfile->real_file = file;
    streamfile->current_physical_offset = start_offset;
    streamfile->start_physical_offset = start_offset;
    streamfile->current_logical_offset = 0;
    streamfile->current_block_size = 0;
    streamfile->stream_id = stream_id;

    return &streamfile->sf;
}

#endif /* _AIX_STREAMFILE_H_ */
