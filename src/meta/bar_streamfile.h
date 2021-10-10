#ifndef _BAR_STREAMFILE_H_
#define _BAR_STREAMFILE_H_
#include "../streamfile.h"

/* a streamfile wrapping another for decryption */

enum {BAR_KEY_LENGTH = 16};

// don't know if this is unique, but seems accurate
static const uint8_t bar_key[BAR_KEY_LENGTH] = {
        0xbd,0x14,0x0e,0x0a,0x91,0xeb,0xaa,0xf6,
        0x11,0x44,0x17,0xc2,0x1c,0xe4,0x66,0x80
};

typedef struct _BARSTREAMFILE {
    STREAMFILE sf;
    STREAMFILE *real_file;
} BARSTREAMFILE;


/*static*/ STREAMFILE *wrap_bar_STREAMFILE(STREAMFILE *file);


static size_t read_bar(BARSTREAMFILE *streamFile, uint8_t *dest, offv_t offset, size_t length) {
    off_t i;
    size_t read_length = streamFile->real_file->read(streamFile->real_file, dest, offset, length);

    for (i = 0; i < read_length; i++) {
        dest[i] = dest[i] ^ bar_key[(i+offset)%BAR_KEY_LENGTH];
    }

    return read_length;
}

static size_t get_size_bar(BARSTREAMFILE *streamFile) {
    return streamFile->real_file->get_size(streamFile->real_file);
}

static offv_t get_offset_bar(BARSTREAMFILE *streamFile) {
    return streamFile->real_file->get_offset(streamFile->real_file);
}

static void get_name_bar(BARSTREAMFILE *streamFile, char *name, size_t length) {
    streamFile->real_file->get_name(streamFile->real_file, name, length);
}

static STREAMFILE *open_bar(BARSTREAMFILE *streamFile, const char * const filename, size_t buffersize) {
    STREAMFILE *newfile = streamFile->real_file->open(streamFile->real_file,filename,buffersize);
    if (!newfile)
        return NULL;

    return wrap_bar_STREAMFILE(newfile);
}

static void close_bar(BARSTREAMFILE *streamFile) {
    streamFile->real_file->close(streamFile->real_file);
    free(streamFile);
    return;
}


/*static*/ STREAMFILE *wrap_bar_STREAMFILE(STREAMFILE *file) {
    BARSTREAMFILE *streamfile = malloc(sizeof(BARSTREAMFILE));

    if (!streamfile)
        return NULL;

    memset(streamfile, 0, sizeof(BARSTREAMFILE));

    streamfile->sf.read = (void*)read_bar;
    streamfile->sf.get_size = (void*)get_size_bar;
    streamfile->sf.get_offset = (void*)get_offset_bar;
    streamfile->sf.get_name = (void*)get_name_bar;
    streamfile->sf.open = (void*)open_bar;
    streamfile->sf.close = (void*)close_bar;

    streamfile->real_file = file;

    return &streamfile->sf;
}

#endif /* _BAR_STREAMFILE_H_ */
