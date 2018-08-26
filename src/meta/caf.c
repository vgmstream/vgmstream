#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* CAF - from tri-Crescendo games [Baten Kaitos 1/2 (GC), Fragile (Wii)] */
VGMSTREAM * init_vgmstream_caf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, offset;
    size_t file_size;
    int channel_count, loop_flag;
    int32_t num_samples = 0;
    uint32_t loop_start = -1;


    /* checks */
    /* .caf: header id, .cfn: fake extension? , "" is accepted as files don't have extensions in the disc */
    if (!check_extensions(streamFile,"caf,cfn,"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x43414620) /* "CAF " */
        goto fail;

    /* get total samples */
    offset = 0;
    file_size = get_streamfile_size(streamFile);
    while (offset < file_size) {
        off_t next_block = read_32bitBE(offset+0x04,streamFile);
        num_samples += read_32bitBE(offset+0x14,streamFile)/8*14;

        if(read_32bitBE(offset+0x20,streamFile)==read_32bitBE(offset+0x08,streamFile)) {
            loop_start = num_samples - read_32bitBE(offset+0x14,streamFile)/8*14;
        }
        offset += next_block;
    }

    start_offset = 0x00;
    channel_count = 2; /* always stereo */
    loop_flag = (loop_start!=-1);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 32000;
    vgmstream->num_samples = num_samples;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        vgmstream->loop_end_sample = num_samples;
    }

    vgmstream->meta_type = meta_CAF;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_caf;

    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
