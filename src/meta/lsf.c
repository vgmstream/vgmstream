#include "meta.h"
#include "../util.h"

/* .lsf - from Atod games [Fastlane Street Racing (iPhone), Chicane Street Racing prototype (Gizmondo)] */
VGMSTREAM * init_vgmstream_lsf_n1nj4n(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t file_size;


    /* checks */
    if (!check_extensions(streamFile, "lsf"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x216E316E || // "!n1n"
        read_32bitBE(0x04, streamFile) != 0x6A346E00)   // "j4n\0"
        goto fail;

    file_size = get_streamfile_size(streamFile);
    if (read_32bitLE(0x0C, streamFile) + 0x10 != file_size)
        goto fail;

    loop_flag = 0;
    channel_count = 1;
    start_offset = 0x10;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LSF_N1NJ4N;
    vgmstream->sample_rate = read_32bitLE(0x08, streamFile);
    vgmstream->num_samples = (file_size-0x10)/0x1c*0x1b*2;
    vgmstream->coding_type = coding_LSF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x1c;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
