#include "streamfile.h"
#include "util.h"

STREAMFILE * open_streamfile(const char * const filename) {
    return open_streamfile_buffer(filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
}

STREAMFILE * open_streamfile_buffer(const char * const filename, size_t buffersize) {
    FILE * infile;
    uint8_t * buffer;
    STREAMFILE * streamfile;
   
    infile = fopen(filename,"rb");
    if (!infile) return NULL;

    buffer = calloc(buffersize,1);
    if (!buffer) {
        fclose(infile);
        return NULL;
    }

    streamfile = calloc(1,sizeof(STREAMFILE));
    if (!streamfile) {
        fclose(infile);
        free(buffer);
        return NULL;
    }

    streamfile->infile = infile;
    streamfile->buffersize = buffersize;
    streamfile->buffer = buffer;

    return streamfile;
}

void close_streamfile(STREAMFILE * streamfile) {
    fclose(streamfile->infile);
    free(streamfile->buffer);
    free(streamfile);
}

size_t read_the_rest(uint8_t * dest, off_t offset, size_t length, STREAMFILE * streamfile) {
    size_t length_read_total=0;

    /* is the beginning at least there? */
    if (offset >= streamfile->offset && offset < streamfile->offset+streamfile->validsize) {
        size_t length_read;
        off_t offset_into_buffer = offset-streamfile->offset;
        length_read = streamfile->validsize-offset_into_buffer;
        memcpy(dest,streamfile->buffer+offset_into_buffer,length_read);
        length_read_total += length_read;
        length -= length_read;
        offset += length_read;
        dest += length_read;
    }

    /* TODO: What would make more sense here is to read the whole request
     * at once into the dest buffer, as it must be large enough, and then
     * copy some part of that into our own buffer.
     * The destination buffer is supposed to be much smaller than the
     * STREAMFILE buffer, though. Maybe we should only ever return up
     * to the buffer size to avoid having to deal with things like this
     * which are outside of my intended use.
     */
    /* read as much of the beginning of the request as possible, proceed */
    while (length>0) {
        size_t length_to_read;
        size_t length_read=0;
        streamfile->validsize=0;
        if (fseeko(streamfile->infile,offset,SEEK_SET)) return length_read;
        streamfile->offset=offset;

        /* decide how much must be read this time */
        if (length>streamfile->buffersize) length_to_read=streamfile->buffersize;
        else length_to_read=length;

        /* always try to fill the buffer */
        length_read = fread(streamfile->buffer,1,streamfile->buffersize,streamfile->infile);
        streamfile->validsize=length_read;

        /* if we can't get enough to satisfy the request we give up */
        if (length_read < length_to_read) {
            memcpy(dest,streamfile->buffer,length_read);
            length_read_total+=length_read;
            return length_read_total;
        }

        /* use the new buffer */
        memcpy(dest,streamfile->buffer,length_to_read);
        length_read_total+=length_to_read;
        length-=length_to_read;
        dest+=length_to_read;
        offset+=length_to_read;
    }

    return length_read_total;
}

size_t get_streamfile_size(STREAMFILE * streamfile) {
    fseeko(streamfile->infile,0,SEEK_END);
    return ftello(streamfile->infile);
}
