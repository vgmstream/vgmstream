#include "streamfile.h"
#include "util.h"

typedef struct {
    STREAMFILE sf;
    FILE * infile;
    off_t offset;
    size_t validsize;
    uint8_t * buffer;
    size_t buffersize;
    char name[260];
} STDIOSTREAMFILE;

static STREAMFILE * open_stdio_streamfile_buffer_by_FILE(FILE *infile,const char * const filename, size_t buffersize);

static size_t read_the_rest(uint8_t * dest, off_t offset, size_t length, STDIOSTREAMFILE * streamfile) {
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

static size_t read_stdio(STDIOSTREAMFILE *streamfile,uint8_t * dest, off_t offset, size_t length)
{
    // read
    if (!streamfile || !dest || length<=0) return 0;

    /* if entire request is within the buffer */
    if (offset >= streamfile->offset && offset+length <= streamfile->offset+streamfile->validsize) {
        memcpy(dest,streamfile->buffer+(offset-streamfile->offset),length);
        return length;
    }

    return read_the_rest(dest,offset,length,streamfile);
}

static void close_stdio(STDIOSTREAMFILE * streamfile) {
    fclose(streamfile->infile);
    free(streamfile->buffer);
    free(streamfile);
}

static size_t get_size_stdio(STDIOSTREAMFILE * streamfile) {
    fseeko(streamfile->infile,0,SEEK_END);
    return ftello(streamfile->infile);
}

static off_t get_offset_stdio(STDIOSTREAMFILE *streamFile) {
    return streamFile->offset;
}

static void get_name_stdio(STDIOSTREAMFILE *streamfile,char *buffer,size_t length) {
    strcpy(buffer,streamfile->name);
}

static STREAMFILE *open_stdio(STDIOSTREAMFILE *streamFile,const char * const filename,size_t buffersize) {
    int newfd;
    FILE *newfile;
    STREAMFILE *newstreamFile;

    if (!filename)
        return NULL;
    // if same name, duplicate the file pointer we already have open
    if (!strcmp(streamFile->name,filename)) {
        if (((newfd = dup(fileno(streamFile->infile))) >= 0) &&
            (newfile = fdopen( newfd, "rb" ))) 
        {
            newstreamFile = open_stdio_streamfile_buffer_by_FILE(newfile,filename,buffersize);
            if (newstreamFile) { 
                return newstreamFile;
            }
            // failure, close it and try the default path (which will probably fail a second time)
            fclose(newfile);
        }
    }
    // a normal open, open a new file
    return open_stdio_streamfile_buffer(filename,buffersize);
}

static STREAMFILE * open_stdio_streamfile_buffer_by_FILE(FILE *infile,const char * const filename, size_t buffersize) {
    uint8_t * buffer;
    STDIOSTREAMFILE * streamfile;

    buffer = calloc(buffersize,1);
    if (!buffer) {
        return NULL;
    }

    streamfile = calloc(1,sizeof(STDIOSTREAMFILE));
    if (!streamfile) {
        free(buffer);
        return NULL;
    }

    streamfile->sf.read = (void*)read_stdio;
    streamfile->sf.get_size = (void*)get_size_stdio;
    streamfile->sf.get_offset = (void*)get_offset_stdio;
    streamfile->sf.get_name = (void*)get_name_stdio;
    streamfile->sf.open = (void*)open_stdio;
    streamfile->sf.close = (void*)close_stdio;

    streamfile->infile = infile;
    streamfile->buffersize = buffersize;
    streamfile->buffer = buffer;

    strcpy(streamfile->name,filename);

    return &streamfile->sf;
}

STREAMFILE * open_stdio_streamfile_buffer(const char * const filename, size_t buffersize) {
    FILE * infile;
    STREAMFILE *streamFile;

    infile = fopen(filename,"rb");
    if (!infile) return NULL;

    streamFile = open_stdio_streamfile_buffer_by_FILE(infile,filename,buffersize);
    if (!streamFile) {
        fclose(infile);
    }

    return streamFile;
}