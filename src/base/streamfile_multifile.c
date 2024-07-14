#include "../streamfile.h"
#include "../util/vgmstream_limits.h"


typedef struct {
    STREAMFILE vt;

    STREAMFILE** inner_sfs;
    size_t inner_sfs_size;
    size_t *sizes;
    offv_t size;
    offv_t offset;
} MULTIFILE_STREAMFILE;

static size_t multifile_read(MULTIFILE_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    int i, segment = 0;
    offv_t segment_offset = 0;
    size_t done = 0;

    if (offset > sf->size) {
        sf->offset = sf->size;
        return 0;
    }

    /* map external offset to multifile offset */
    for (i = 0; i < sf->inner_sfs_size; i++) {
        size_t segment_size = sf->sizes[i];
        /* check if offset falls in this segment */
        if (offset >= segment_offset && offset < segment_offset + segment_size) {
            segment = i;
            segment_offset = offset - segment_offset;
            break;
        }

        segment_offset += segment_size;
    }

    /* reads can span multiple segments */
    while(done < length) {
        if (segment >= sf->inner_sfs_size) /* over last segment, not fully done */
            break;
        /* reads over segment size are ok, will return smaller value and continue next segment */
        done += sf->inner_sfs[segment]->read(sf->inner_sfs[segment], dst + done, segment_offset, length - done);
        segment++;
        segment_offset = 0;
    }

    sf->offset = offset + done;
    return done;
}

static size_t multifile_get_size(MULTIFILE_STREAMFILE* sf) {
    return sf->size;
}

static offv_t multifile_get_offset(MULTIFILE_STREAMFILE* sf) {
    return sf->offset;
}

static void multifile_get_name(MULTIFILE_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sfs[0]->get_name(sf->inner_sfs[0], name, name_size);
}

static STREAMFILE* multifile_open(MULTIFILE_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    char original_filename[PATH_LIMIT];
    STREAMFILE* new_sf = NULL;
    STREAMFILE** new_inner_sfs = NULL;
    int i;

    sf->inner_sfs[0]->get_name(sf->inner_sfs[0], original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) { /* same multifile */
        new_inner_sfs = calloc(sf->inner_sfs_size, sizeof(STREAMFILE*));
        if (!new_inner_sfs) goto fail;

        for (i = 0; i < sf->inner_sfs_size; i++) {
            sf->inner_sfs[i]->get_name(sf->inner_sfs[i], original_filename, PATH_LIMIT);
            new_inner_sfs[i] = sf->inner_sfs[i]->open(sf->inner_sfs[i], original_filename, buf_size);
            if (!new_inner_sfs[i]) goto fail;
        }

        new_sf = open_multifile_streamfile(new_inner_sfs, sf->inner_sfs_size);
        if (!new_sf) goto fail;

        free(new_inner_sfs);
        return new_sf;
    }
    else {
        return sf->inner_sfs[0]->open(sf->inner_sfs[0], filename, buf_size); /* regular file */
    }

fail:
    if (new_inner_sfs) {
        for (i = 0; i < sf->inner_sfs_size; i++)
            close_streamfile(new_inner_sfs[i]);
    }
    free(new_inner_sfs);
    return NULL;
}

static void multifile_close(MULTIFILE_STREAMFILE* sf) {
    int i;
    for (i = 0; i < sf->inner_sfs_size; i++) {
        for (i = 0; i < sf->inner_sfs_size; i++) {
            close_streamfile(sf->inner_sfs[i]);
        }
    }
    free(sf->inner_sfs);
    free(sf->sizes);
    free(sf);
}


STREAMFILE* open_multifile_streamfile(STREAMFILE** sfs, size_t sfs_size) {
    MULTIFILE_STREAMFILE* this_sf = NULL;
    int i;

    if (!sfs || !sfs_size) return NULL;

    for (i = 0; i < sfs_size; i++) {
        if (!sfs[i]) return NULL;
    }

    this_sf = calloc(1, sizeof(MULTIFILE_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)multifile_read;
    this_sf->vt.get_size = (void*)multifile_get_size;
    this_sf->vt.get_offset = (void*)multifile_get_offset;
    this_sf->vt.get_name = (void*)multifile_get_name;
    this_sf->vt.open = (void*)multifile_open;
    this_sf->vt.close = (void*)multifile_close;
    this_sf->vt.stream_index = sfs[0]->stream_index;

    this_sf->inner_sfs_size = sfs_size;
    this_sf->inner_sfs = calloc(sfs_size, sizeof(STREAMFILE*));
    if (!this_sf->inner_sfs) goto fail;
    this_sf->sizes = calloc(sfs_size, sizeof(size_t));
    if (!this_sf->sizes) goto fail;

    for (i = 0; i < this_sf->inner_sfs_size; i++) {
        this_sf->inner_sfs[i] = sfs[i];
        this_sf->sizes[i] = sfs[i]->get_size(sfs[i]);
        this_sf->size += this_sf->sizes[i];
    }

    return &this_sf->vt;

fail:
    if (this_sf) {
        free(this_sf->inner_sfs);
        free(this_sf->sizes);
    }
    free(this_sf);
    return NULL;
}

STREAMFILE* open_multifile_streamfile_f(STREAMFILE** sfs, size_t sfs_size) {
    STREAMFILE* new_sf = open_multifile_streamfile(sfs, sfs_size);
    if (!new_sf) {
        int i;
        for (i = 0; i < sfs_size; i++) {
            close_streamfile(sfs[i]);
        }
    }
    return new_sf;
}
