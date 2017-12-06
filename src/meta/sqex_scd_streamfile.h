#ifndef _SQEX_SCD_STREAMFILE_H_
#define _SQEX_SCD_STREAMFILE_H_
#include "../streamfile.h"

/* special streamfile type to handle deinterleaving of complete files (based heavily on AIXSTREAMFILE */

typedef struct _SCDINTSTREAMFILE {
    STREAMFILE sf;
    STREAMFILE *real_file;
    const char * filename;
    off_t start_physical_offset;
    off_t current_logical_offset;
    off_t interleave_block_size;
    off_t stride_size;
    size_t total_size;
} SCDINTSTREAMFILE;


/*static*/ STREAMFILE *open_scdint_with_STREAMFILE(STREAMFILE *file, const char * filename, off_t start_offset, off_t interleave_block_size, off_t stride_size, size_t total_size);


static STREAMFILE *open_scdint_impl(SCDINTSTREAMFILE *streamfile,const char * const filename,size_t buffersize) {
    SCDINTSTREAMFILE *newfile;

    if (strcmp(filename, streamfile->filename))
        return NULL;

    newfile = malloc(sizeof(SCDINTSTREAMFILE));
    if (!newfile)
        return NULL;

    memcpy(newfile,streamfile,sizeof(SCDINTSTREAMFILE));
    return &newfile->sf;
}

static void close_scdint(SCDINTSTREAMFILE *streamfile) {
    free(streamfile);
    return;
}

static size_t get_size_scdint(SCDINTSTREAMFILE *streamfile) {
    return streamfile->total_size;
}

static size_t get_offset_scdint(SCDINTSTREAMFILE *streamfile) {
    return streamfile->current_logical_offset;
}

static void get_name_scdint(SCDINTSTREAMFILE *streamfile, char *buffer, size_t length) {
    strncpy(buffer,streamfile->filename,length);
    buffer[length-1]='\0';
}

static size_t read_scdint(SCDINTSTREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length) {
    size_t sz = 0;

    while (length > 0) {
        off_t to_read;
        off_t length_available;
        off_t block_num;
        off_t intrablock_offset;
        off_t physical_offset;


        block_num = offset / streamfile->interleave_block_size;
        intrablock_offset = offset % streamfile->interleave_block_size;
        streamfile->current_logical_offset = offset;
        physical_offset = streamfile->start_physical_offset + block_num * streamfile->stride_size + intrablock_offset;

        length_available = streamfile->interleave_block_size - intrablock_offset;

        if (length < length_available) {
            to_read = length;
        }
        else {
            to_read = length_available;
        }

        if (to_read > 0) {
            size_t bytes_read;

            bytes_read = read_streamfile(dest,
                physical_offset,
                to_read, streamfile->real_file);

            sz += bytes_read;

            streamfile->current_logical_offset = offset + bytes_read;

            if (bytes_read != to_read) {
                return sz; /* an error which we will not attempt to handle here */
            }

            dest += bytes_read;
            offset += bytes_read;
            length -= bytes_read;
        }
    }

    return sz;
}

/* start_offset is for *this* interleaved stream */
/*static*/ STREAMFILE *open_scdint_with_STREAMFILE(STREAMFILE *file, const char * filename, off_t start_offset, off_t interleave_block_size, off_t stride_size, size_t total_size) {
    SCDINTSTREAMFILE * scd = NULL;

    /* _scdint funcs can't handle this case */
    if (start_offset + total_size > file->get_size(file))
        return NULL;

    scd = malloc(sizeof(SCDINTSTREAMFILE));
    if (!scd)
        return NULL;

    scd->sf.read = (void*)read_scdint;
    scd->sf.get_size = (void*)get_size_scdint;
    scd->sf.get_offset = (void*)get_offset_scdint;
    scd->sf.get_name = (void*)get_name_scdint;
    scd->sf.get_realname = (void*)get_name_scdint;
    scd->sf.open = (void*)open_scdint_impl;
    scd->sf.close = (void*)close_scdint;

    scd->real_file = file;
    scd->filename = filename;
    scd->start_physical_offset = start_offset;
    scd->current_logical_offset = 0;
    scd->interleave_block_size = interleave_block_size;
    scd->stride_size = stride_size;
    scd->total_size = total_size;

    return &scd->sf;
}

#endif /* _SCD_STREAMFILE_H_ */
