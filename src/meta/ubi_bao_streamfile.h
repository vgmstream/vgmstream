#ifndef _UBI_BAO_STREAMFILE_H_
#define _UBI_BAO_STREAMFILE_H_

//todo fix dupe code, but would be nice to keep it all in separate compilation units
#include "ubi_sb_streamfile.h"

static STREAMFILE* setup_ubi_bao_streamfile(STREAMFILE *streamFile, off_t stream_offset, size_t stream_size, int layer_number, int layer_count, int big_endian) {
    return setup_ubi_sb_streamfile(streamFile, stream_offset, stream_size, layer_number, layer_count, big_endian, 0);
}

#endif /* _UBI_BAO_STREAMFILE_H_ */
