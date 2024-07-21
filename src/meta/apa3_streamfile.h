#ifndef _APA3_STREAMFILE_H_
#define _APA3_STREAMFILE_H_
#include "meta.h"


#define ATX_MAX_SEGMENTS  2


static STREAMFILE* setup_apa3_streamfile(STREAMFILE* sf) {
    STREAMFILE* new_sf = NULL;
    STREAMFILE* segments_sf[ATX_MAX_SEGMENTS] = {0};
    char filename[PATH_LIMIT];
    size_t riff_size;


    /* this must be first segment */
    if (read_u16le(0x1c,sf) != 0)
        return NULL;
    int total_segments = read_u16le(0x1e,sf);
    if (total_segments < 1 || total_segments > ATX_MAX_SEGMENTS)
        return NULL;

    /* expected segment name: X_XXX_XXX_0n.ATX, starting from n=1 */
    get_streamfile_filename(sf, filename, PATH_LIMIT);
    size_t filename_len = strlen(filename);
    if (filename_len < 7 || filename[filename_len - 5] != '1')
        return NULL;

    /* setup segments (could avoid reopening first segment but meh) */
    for (int i = 0; i < total_segments; i++) {
        off_t subfile_offset;
        size_t subfile_size;

        filename[filename_len - 5] = ('0' + i + 1); /* digit conversion */

        segments_sf[i] = open_streamfile_by_filename(sf, filename);
        if (!segments_sf[i]) goto fail;

        if (!is_id32be(0x00, segments_sf[i], "APA3"))
            goto fail;

        /* parse block/segment header (other Media.Vision's files use it too) */
        subfile_offset = read_32bitLE(0x08, segments_sf[i]); /* header size */
        subfile_size = read_32bitLE(0x14, segments_sf[i]); /* can be 0 in other containers */

        if (read_u16le(0x1c,segments_sf[i]) != i)
            goto fail; /* segment sequence */
        // 0x04: block size (should match subfile_size in .ATX)
        // 0x0c: flags? also in other files
        // 0x10/18: null
        // 0x1e: total segments

        /* clamp to ignore header/footer during next reads */
        segments_sf[i] = open_clamp_streamfile_f(segments_sf[i], subfile_offset, subfile_size);
        if (!segments_sf[i]) goto fail;
    }

    /* setup with all segments and clamp further using riff_size (last segment has padding) */
    riff_size = read_32bitLE(read_32bitLE(0x08,sf) + 0x04,sf) + 0x08;

    new_sf = open_multifile_streamfile_f(segments_sf, total_segments);
    new_sf = open_clamp_streamfile_f(new_sf, 0, riff_size);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "at3");

    /* if all this worked we'll have this frankenstein streamfile:
     *  fakename( clamp( multifile( segment0=clamp(standard(FILE)), segment1=clamp(standard(FILE)) ) ) ) */

    return new_sf;
fail:
    if (!new_sf) {
        for (int i = 0; i < total_segments; i++) {
            close_streamfile(segments_sf[i]);
        }
    } else {
        close_streamfile(new_sf); /* closes all segments */
    }

    return NULL;
}

#endif
