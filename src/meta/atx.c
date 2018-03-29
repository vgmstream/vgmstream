#include "meta.h"
#include "../coding/coding.h"

#define ATX_MAX_SEGMENTS  2

static STREAMFILE* setup_atx_streamfile(STREAMFILE *streamFile);

/* .ATX - Media.Vision's segmented RIFF AT3 wrapper [Senjo no Valkyria 3 (PSP), Shining Blade (PSP)] */
VGMSTREAM * init_vgmstream_atx(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;


    /* check extensions */
    if ( !check_extensions(streamFile,"atx"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41504133) /* "APA3" */
        goto fail;

    /* .ATX is made of subfile segments, handled by the streamFile.
     * Each segment has a header/footer, and part of the whole data
     * (i.e. ATRAC3 data ends in a subfile and continues in the next) */
    temp_streamFile = setup_atx_streamfile(streamFile);
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_riff(temp_streamFile);
    if (!vgmstream) goto fail;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

static STREAMFILE* setup_atx_streamfile(STREAMFILE *streamFile) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    STREAMFILE *segment_streamFiles[ATX_MAX_SEGMENTS] = {0};
    char filename[PATH_LIMIT];
    size_t filename_len;
    int i, num_segments = 0;
    size_t riff_size;


    if (read_16bitLE(0x1c,streamFile) != 0) goto fail; /* this must be first segment */
    if (read_16bitLE(0x1e,streamFile) < 1 || read_16bitLE(0x1e,streamFile) > ATX_MAX_SEGMENTS) goto fail;
    num_segments = read_16bitLE(0x1e,streamFile);

    /* expected segment name: X_XXX_XXX_0n.ATX, starting from n=1 */
    get_streamfile_filename(streamFile, filename,PATH_LIMIT);
    filename_len = strlen(filename);
    if (filename_len < 7 || filename[filename_len - 5] != '1') goto fail;

    /* setup segments (could avoid reopening first segment but meh) */
    for (i = 0; i < num_segments; i++) {
        off_t subfile_offset;
        size_t subfile_size;

        filename[filename_len - 5] = ('0'+i+1); /* ghetto digit conversion */
        new_streamFile = open_streamfile_by_filename(streamFile, filename);
        if (!new_streamFile) goto fail;
        segment_streamFiles[i] = new_streamFile;

        if (read_32bitBE(0x00,segment_streamFiles[i]) != 0x41504133) /* "APA3" */
            goto fail;

        /* parse block/segment header (other Media.Vision's files use it too) */
        subfile_offset = read_32bitLE(0x08,segment_streamFiles[i]); /* header size */
        subfile_size = read_32bitLE(0x14,segment_streamFiles[i]); /* can be 0 in other containers */

        if (read_16bitLE(0x1c,segment_streamFiles[i]) != i)
            goto fail; /* segment sequence */
        /* 0x04: block size (should match subfile_size in .ATX) */
        /* 0x0c: flags? also in other files, 0x10/18: null, 0x1e: segments */

        /* clamp to ignore header/footer during next reads */
        new_streamFile = open_clamp_streamfile(segment_streamFiles[i], subfile_offset,subfile_size);
        if (!new_streamFile) goto fail;
        segment_streamFiles[i] = new_streamFile;
    }

    /* setup with all segments and clamp further using riff_size (last segment has padding) */
    riff_size = read_32bitLE(read_32bitLE(0x08,streamFile) + 0x04,streamFile) + 0x08;

    new_streamFile = open_multifile_streamfile(segment_streamFiles, num_segments);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, 0,riff_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL, "at3");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    /* if all this worked we'll have this frankenstein streamfile:
     *  fakename( clamp( multifile( segment0=clamp(standard(FILE)), segment1=clamp(standard(FILE)) ) ) ) */

    return temp_streamFile;

fail:
    if (!temp_streamFile) {
        for (i = 0; i < num_segments; i++) {
            close_streamfile(segment_streamFiles[i]);
        }
    } else {
        close_streamfile(temp_streamFile); /* closes all segments */
    }
    return NULL;
}
