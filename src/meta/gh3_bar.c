#include "meta.h"

// Guitar Hero III Mobile .bar

enum {BAR_KEY_LENGTH = 16};

// don't know if this is unique, but seems accurate
static const uint8_t bar_key[BAR_KEY_LENGTH] =
   {0xbd,0x14,0x0e,0x0a,0x91,0xeb,0xaa,0xf6,
    0x11,0x44,0x17,0xc2,0x1c,0xe4,0x66,0x80};

typedef struct _BARSTREAM
{
    STREAMFILE sf;
    STREAMFILE *real_file;
} BARSTREAM;

STREAMFILE *wrap_bar_STREAMFILE(STREAMFILE *file);

VGMSTREAM * init_vgmstream_gh3_bar(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    // don't close, this is just the source streamFile wrapped
    STREAMFILE* streamFileBAR = NULL;
    char filename[260];
    off_t start_offset;
    off_t ch2_start_offset;
    int loop_flag;
	int channel_count;
    long file_size;


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bar",filename_extension(filename))) goto fail;

    /* decryption wrapper for header reading */
    streamFileBAR = wrap_bar_STREAMFILE(streamFile);
    if (!streamFileBAR) goto fail;

    file_size = get_streamfile_size(streamFileBAR);

    /* check header */
    if (read_32bitBE(0x00,streamFileBAR) != 0x11000100 ||
        read_32bitBE(0x04,streamFileBAR) != 0x01000200) goto fail;
    if (read_32bitLE(0x50,streamFileBAR) != file_size) goto fail;

    start_offset = read_32bitLE(0x18,streamFileBAR);
    if (0x54 != start_offset) goto fail;
    ch2_start_offset = read_32bitLE(0x48,streamFileBAR);
    if (ch2_start_offset >= file_size) goto fail;

    /* build the VGMSTREAM */
    channel_count = 2;
    loop_flag = 0;
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 11025;
    vgmstream->coding_type = coding_IMA;
    vgmstream->num_samples = (file_size-ch2_start_offset)*2;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_GH3_BAR;

    {
        STREAMFILE *file1, *file2;
        file1 = streamFileBAR->open(streamFileBAR,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file1) goto fail;
        file2 = streamFileBAR->open(streamFileBAR,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file2)
        {
            close_streamfile(file1);
            goto fail;
        }
        vgmstream->ch[0].streamfile = file1;
        vgmstream->ch[1].streamfile = file2;
        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;
        vgmstream->ch[1].channel_start_offset=
            vgmstream->ch[1].offset=ch2_start_offset;
    }

    // discard our decrypt wrapper, without closing the original streamfile
    free(streamFileBAR);

    return vgmstream;
fail:
    if (streamFileBAR)
        free(streamFileBAR);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

static size_t read_bar(BARSTREAM *streamFile, uint8_t *dest, off_t offset, size_t length)
{
    off_t i;
    size_t read_length =
        streamFile->real_file->read(streamFile->real_file, dest, offset, length);

    for (i = 0; i < read_length; i++)
    {
        dest[i] = dest[i] ^ bar_key[(i+offset)%BAR_KEY_LENGTH];
    }

    return read_length;
}

static size_t get_size_bar(BARSTREAM *streamFile)
{
    return streamFile->real_file->get_size(streamFile->real_file);
}

static size_t get_offset_bar(BARSTREAM *streamFile)
{
    return streamFile->real_file->get_offset(streamFile->real_file);
}

static void get_name_bar(BARSTREAM *streamFile, char *name, size_t length)
{
    return streamFile->real_file->get_name(streamFile->real_file, name, length);
}

static void get_realname_bar(BARSTREAM *streamFile, char *name, size_t length)
{
    return streamFile->real_file->get_realname(streamFile->real_file, name, length);
}

STREAMFILE *open_bar(BARSTREAM *streamFile, const char * const filename, size_t buffersize)
{
    STREAMFILE *newfile = streamFile->real_file->open(
            streamFile->real_file,filename,buffersize);
    if (!newfile)
        return NULL;

    return wrap_bar_STREAMFILE(newfile);
}

static void close_bar(BARSTREAM *streamFile)
{
    streamFile->real_file->close(streamFile->real_file);
    free(streamFile);
    return;
}

#ifdef PROFILE_STREAMFILE
size_t get_bytes_read_bar(BARSTREAM *streamFile)
{
    return streamFile->real_file->get_bytes_read(streamFile->real_file);
}

int (*get_error_count)(BARSTREAM *streamFile)
{
    return streamFile->real_file->get_error_count(streamFile->real_file);
}
#endif

STREAMFILE *wrap_bar_STREAMFILE(STREAMFILE *file)
{
    BARSTREAM *streamfile = malloc(sizeof(BARSTREAM));

    if (!streamfile)
        return NULL;
    
    memset(streamfile, 0, sizeof(BARSTREAM));

    streamfile->sf.read = (void*)read_bar;
    streamfile->sf.get_size = (void*)get_size_bar;
    streamfile->sf.get_offset = (void*)get_offset_bar;
    streamfile->sf.get_name = (void*)get_name_bar;
    streamfile->sf.get_realname = (void*)get_realname_bar;
    streamfile->sf.open = (void*)open_bar;
    streamfile->sf.close = (void*)close_bar;
#ifdef PROFILE_STREAMFILE
    streamfile->sf.get_bytes_read = get_bytes_read_bar;
    streamfile->sf.get_error_count = get_error_count_bar;
#endif

    streamfile->real_file = file;

    return &streamfile->sf;
}
