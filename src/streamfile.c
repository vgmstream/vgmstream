#ifndef _MSC_VER
#include <unistd.h>
#endif
#include "streamfile.h"
#include "util.h"
#include "vgmstream.h"


/* On EOF reads we can return length 0, or ignore and return the requested length + 0-set the buffer.
 * Some decoders don't check for EOF and may decode garbage if returned 0, as read_Nbit() funcs return -1.
 * Only matters for metas that get num_samples wrong (bigger than total data). */
#define STREAMFILE_IGNORE_EOF 0


/* buffered file reader */
typedef struct {
    STREAMFILE sf;          /* callbacks */
    FILE * infile;          /* actual FILE */
    char name[PATH_LIMIT];
    off_t offset;           /* current offset */
    size_t validsize;       /* current buffer size */
    uint8_t * buffer;       /* data buffer */
    size_t buffersize;      /* max buffer size */
    size_t filesize;        /* cached file size (max offset) */
#ifdef VGM_DEBUG_OUTPUT
    int error_notified;
#endif
#ifdef PROFILE_STREAMFILE
    size_t bytes_read;
    int error_count;
#endif
} STDIOSTREAMFILE;

static STREAMFILE * open_stdio_streamfile_buffer_by_FILE(FILE *infile,const char * const filename, size_t buffersize);

static size_t read_the_rest(uint8_t * dest, off_t offset, size_t length, STDIOSTREAMFILE * streamfile) {
    size_t length_read_total=0;

    /* is the part of the requested length in the buffer? */
    if (offset >= streamfile->offset && offset < streamfile->offset + streamfile->validsize) {
        size_t length_read;
        off_t offset_into_buffer = offset - streamfile->offset;
        length_read = streamfile->validsize - offset_into_buffer;

        memcpy(dest,streamfile->buffer + offset_into_buffer,length_read);
        length_read_total += length_read;
        length -= length_read;
        offset += length_read;
        dest += length_read;
    }

    /* What would make more sense here is to read the whole request
     * at once into the dest buffer, as it must be large enough, and then
     * copy some part of that into our own buffer.
     * The destination buffer is supposed to be much smaller than the
     * STREAMFILE buffer, though. Maybe we should only ever return up
     * to the buffer size to avoid having to deal with things like this
     * which are outside of my intended use.
     */
    /* read the rest of the requested length */
    while (length > 0) {
        size_t length_to_read;
        size_t length_read;
        streamfile->validsize = 0; /* buffer is empty now */

        /* request outside file: ignore to avoid seek/read */
        if (offset > streamfile->filesize) {
            streamfile->offset = streamfile->filesize;

#ifdef VGM_DEBUG_OUTPUT
            if (!streamfile->error_notified) {
                VGM_LOG("ERROR: reading over filesize 0x%x @ 0x%lx + 0x%x (buggy meta?)\n", streamfile->filesize, offset, length);
                streamfile->error_notified = 1;
            }
#endif

#if STREAMFILE_IGNORE_EOF
            memset(dest,0,length); /* dest is already shifted */
            return length_read_total + length; /* partially-read + 0-set buffer */
#else
            return length_read_total; /* partially-read buffer */
#endif
        }

        /* position to new offset */
        if (fseeko(streamfile->infile,offset,SEEK_SET)) {
            streamfile->offset = streamfile->filesize;
#ifdef PROFILE_STREAMFILE
            streamfile->error_count++;
#endif
            return 0; /* fail miserably (fseek shouldn't fail and reach this) */
        }
        streamfile->offset = offset;

        /* decide how much must be read this time */
        if (length > streamfile->buffersize)
            length_to_read = streamfile->buffersize;
        else
            length_to_read = length;

        /* fill the buffer */
        length_read = fread(streamfile->buffer,sizeof(uint8_t),streamfile->buffersize,streamfile->infile);
        streamfile->validsize = length_read;

#ifdef PROFILE_STREAMFILE
        if (ferror(streamfile->infile)) {
            clearerr(streamfile->infile);
            streamfile->error_count++;
        }

        streamfile->bytes_read += length_read;
#endif

        /* if we can't get enough to satisfy the request (EOF) we give up */
        if (length_read < length_to_read) {
            memcpy(dest,streamfile->buffer,length_read);

#if STREAMFILE_IGNORE_EOF
            memset(dest+length_read,0,length-length_read);
            return length_read_total + length; /* partially-read + 0-set buffer */
#else
            return length_read_total + length_read; /* partially-read buffer */
#endif
        }

        /* use the new buffer */
        memcpy(dest,streamfile->buffer,length_to_read);
        length_read_total += length_to_read;
        length -= length_to_read;
        dest += length_to_read;
        offset += length_to_read;
    }

    return length_read_total;
}

static size_t read_stdio(STDIOSTREAMFILE *streamfile,uint8_t * dest, off_t offset, size_t length) {

    if (!streamfile || !dest || length<=0)
        return 0;

    /* request outside file: ignore to avoid seek/read in read_the_rest() */
    if (offset > streamfile->filesize) {
        streamfile->offset = streamfile->filesize;

#ifdef VGM_DEBUG_OUTPUT
        if (!streamfile->error_notified) {
            VGM_LOG("ERROR: offset over filesize 0x%x @ 0x%lx + 0x%x (buggy meta?)\n", streamfile->filesize, offset, length);
            streamfile->error_notified = 1;
        }
#endif

#if STREAMFILE_IGNORE_EOF
        memset(dest,0,length);
        return length; /* 0-set buffer */
#else
        return 0; /* nothing to read */
#endif
    }

    /* just copy if entire request is within the buffer */
    if (offset >= streamfile->offset && offset + length <= streamfile->offset + streamfile->validsize) {
        off_t offset_into_buffer = offset - streamfile->offset;
        memcpy(dest,streamfile->buffer + offset_into_buffer,length);
        return length;
    }

    /* request outside buffer: new fread */
    {
        size_t length_read = read_the_rest(dest,offset,length,streamfile);
#if PROFILE_STREAMFILE
        if (length_read < length) 
            streamfile->error_count++;
#endif
        return length_read;
    }
}

static void close_stdio(STDIOSTREAMFILE * streamfile) {
    fclose(streamfile->infile);
    free(streamfile->buffer);
    free(streamfile);
}

static size_t get_size_stdio(STDIOSTREAMFILE * streamfile) {
    return streamfile->filesize;
}

static off_t get_offset_stdio(STDIOSTREAMFILE *streamFile) {
    return streamFile->offset;
}

static void get_name_stdio(STDIOSTREAMFILE *streamfile,char *buffer,size_t length) {
    strncpy(buffer,streamfile->name,length);
    buffer[length-1]='\0';
}

#ifdef PROFILE_STREAMFILE
static size_t get_bytes_read_stdio(STDIOSTREAMFILE *streamFile) {
    return streamFile->bytes_read;
}
static size_t get_error_count_stdio(STDIOSTREAMFILE *streamFile) {
    return streamFile->error_count;
}
#endif

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
    streamfile->sf.get_realname = (void*)get_name_stdio;
    streamfile->sf.open = (void*)open_stdio;
    streamfile->sf.close = (void*)close_stdio;
#ifdef PROFILE_STREAMFILE
    streamfile->sf.get_bytes_read = (void*)get_bytes_read_stdio;
    streamfile->sf.get_error_count = (void*)get_error_count_stdio;
#endif

    streamfile->infile = infile;
    streamfile->buffersize = buffersize;
    streamfile->buffer = buffer;

    strncpy(streamfile->name,filename,sizeof(streamfile->name));
    streamfile->name[sizeof(streamfile->name)-1] = '\0';

    /* cache filesize */
    fseeko(streamfile->infile,0,SEEK_END);
    streamfile->filesize = ftello(streamfile->infile);

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

/* Read a line into dst. The source files are MS-DOS style,
 * separated (not terminated) by CRLF. Return 1 if the full line was
 * retrieved (if it could fit in dst), 0 otherwise. In any case the result
 * will be properly terminated. The CRLF will be removed if there is one.
 * Return the number of bytes read (including CRLF line ending). Note that
 * this is not the length of the string, and could be larger than the buffer.
 * *line_done_ptr is set to 1 if the complete line was read into dst,
 * otherwise it is set to 0. line_done_ptr can be NULL if you aren't
 * interested in this info.
 */
size_t get_streamfile_dos_line(int dst_length, char * dst, off_t offset,
        STREAMFILE * infile, int *line_done_ptr)
{
    int i;
    off_t file_length = get_streamfile_size(infile);
    /* how many bytes over those put in the buffer were read */
    int extra_bytes = 0;

    if (line_done_ptr) *line_done_ptr = 0;

    for (i=0;i<dst_length-1 && offset+i < file_length;i++)
    {
        char in_char = read_8bit(offset+i,infile);
        /* check for end of line */
        if (in_char == 0x0d &&
                read_8bit(offset+i+1,infile) == 0x0a)
        {
            extra_bytes = 2;
            if (line_done_ptr) *line_done_ptr = 1;
            break;
        }

        dst[i]=in_char;
    }
    
    dst[i]='\0';

    /* did we fill the buffer? */
    if (i==dst_length) {
        /* did the bytes we missed just happen to be the end of the line? */
        if (read_8bit(offset+i,infile) == 0x0d &&
                read_8bit(offset+i+1,infile) == 0x0a)
        {
            extra_bytes = 2;
            /* if so be proud! */
            if (line_done_ptr) *line_done_ptr = 1;
        }
    }

    /* did we hit the file end? */
    if (offset+i == file_length)
    {
        /* then we did in fact finish reading the last line */
        if (line_done_ptr) *line_done_ptr = 1;
    }

    return i+extra_bytes;
}


/**
 * Opens an stream using the base streamFile name plus a new extension (ex. for headers in a separate file)
 */
STREAMFILE * open_stream_ext(STREAMFILE *streamFile, const char * ext) {
    char filename_ext[PATH_LIMIT];

    streamFile->get_name(streamFile,filename_ext,sizeof(filename_ext));
    strcpy(filename_ext + strlen(filename_ext) - strlen(filename_extension(filename_ext)), ext);

    return streamFile->open(streamFile,filename_ext,STREAMFILE_DEFAULT_BUFFER_SIZE);
}

/**
 * open file containing decryption keys and copy to buffer
 * tries combinations of keynames based on the original filename
 *
 * returns true if found and copied
 */
int read_key_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile) {
    char keyname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    const char *path, *ext;
    STREAMFILE * streamFileKey = NULL;

    streamFile->get_name(streamFile,filename,sizeof(filename));

    if (strlen(filename)+4 > sizeof(keyname)) goto fail;

    /* try to open a keyfile using variations:
     *  "(name.ext)key" (per song), "(.ext)key" (per folder) */
    {
        ext = strrchr(filename,'.');
        if (ext!=NULL) ext = ext+1;

        path = strrchr(filename,DIR_SEPARATOR);
        if (path!=NULL) path = path+1;

        /* "(name.ext)key" */
        strcpy(keyname, filename);
        strcat(keyname, "key");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;

        /* "(name.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;
        */


        /* "(.ext)key" */
        if (path) {
            strcpy(keyname, filename);
            keyname[path-filename] = '\0';
            strcat(keyname, ".");
        } else {
            strcpy(keyname, ".");
        }
        if (ext) strcat(keyname, ext);
        strcat(keyname, "key");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;

        /* "(.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        streamFileKey = streamFile->open(streamFile,keyname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFileKey) goto found;
        */

        goto fail;
    }

found:
    if (get_streamfile_size(streamFileKey) != bufsize) goto fail;

    if (read_streamfile(buf, 0, bufsize, streamFileKey)!=bufsize) goto fail;

    close_streamfile(streamFileKey);

    return 1;

fail:
    if (streamFileKey) close_streamfile(streamFileKey);

    return 0;
}

/**
 * open file containing looping data and copy to buffer
 *
 * returns true if found and copied
 */
int read_pos_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile) {
    char posname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    /*size_t bytes_read;*/
    STREAMFILE * streamFilePos= NULL;

    streamFile->get_name(streamFile,filename,sizeof(filename));

    if (strlen(filename)+4 > sizeof(posname)) goto fail;

    /* try to open a posfile using variations: "(name.ext).pos" */
    {
        strcpy(posname, filename);
        strcat(posname, ".pos");
        streamFilePos = streamFile->open(streamFile,posname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (streamFilePos) goto found;

        goto fail;
    }

found:
    //if (get_streamfile_size(streamFilePos) != bufsize) goto fail;

    /* allow pos files to be of different sizes in case of new features, just fill all we can */
    memset(buf, 0, bufsize);
    read_streamfile(buf, 0, bufsize, streamFilePos);

    close_streamfile(streamFilePos);

    return 1;

fail:
    if (streamFilePos) close_streamfile(streamFilePos);

    return 0;
}


/**
 * checks if the stream filename is one of the extensions (comma-separated, ex. "adx" or "adx,aix")
 *
 * returns 0 on failure
 */
int check_extensions(STREAMFILE *streamFile, const char * cmp_exts) {
    char filename[PATH_LIMIT];
    const char * ext = NULL;
    const char * cmp_ext = NULL;
    const char * ststr_res = NULL;
    size_t ext_len, cmp_len;

    streamFile->get_name(streamFile,filename,sizeof(filename));
    ext = filename_extension(filename);
    ext_len = strlen(ext);

    cmp_ext = cmp_exts;
    do {
        ststr_res = strstr(cmp_ext, ",");
        cmp_len = ststr_res == NULL
                  ? strlen(cmp_ext) /* total length if more not found */
                  : (intptr_t)ststr_res - (intptr_t)cmp_ext; /* find next ext; ststr_res should always be greater than cmp_ext, resulting in a positive cmp_len */

        if (ext_len == cmp_len && strncasecmp(ext,cmp_ext, ext_len) == 0)
            return 1;

        cmp_ext = ststr_res;
        if (cmp_ext != NULL)
            cmp_ext = cmp_ext + 1; /* skip comma */

    } while (cmp_ext != NULL);

    return 0;
}


/**
 * Find a chunk starting from an offset, and save its offset/size (if not NULL), with offset after id/size.
 * Works for chunked headers in the form of "chunk_id chunk_size (data)"xN  (ex. RIFF).
 * The start_offset should be the first actual chunk (not "RIFF" or "WAVE" but "fmt ").
 * "full_chunk_size" signals chunk_size includes 4+4+data.
 *
 * returns 0 on failure
 */
int find_chunk_be(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(streamFile, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 1, 0);
}
int find_chunk_le(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(streamFile, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 0, 0);
}
int find_chunk(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int size_big_endian, int zero_size_end) {
    size_t filesize;
    off_t current_chunk = start_offset;

    filesize = get_streamfile_size(streamFile);
    /* read chunks */
    while (current_chunk < filesize) {
        uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
        off_t chunk_size = size_big_endian ?
                read_32bitBE(current_chunk+4,streamFile) :
                read_32bitLE(current_chunk+4,streamFile);

        if (chunk_type == chunk_id) {
            if (out_chunk_offset) *out_chunk_offset = current_chunk+8;
            if (out_chunk_size) *out_chunk_size = chunk_size;
            return 1;
        }

        /* empty chunk with 0 size, seen in some formats (XVAG uses it as end marker, Wwise doesn't) */
        if (chunk_size == 0 && zero_size_end)
            return 0;

        current_chunk += full_chunk_size ? chunk_size : 4+4+chunk_size;
    }

    return 0;
}
