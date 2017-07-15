#include <math.h>
#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_ngc_adpdtk(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0;
    int channel_count = 2, loop_flag = 0; /* always stereo, no loop */

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"dtk,adp"))
        goto fail;

    /* .adp files have no header, and the ext is common, so all we can do is look for valid first frames */
    if (check_extensions(streamFile,"adp")) {
        int i;
        for (i = 0; i < 10; i++) { /* try a bunch of frames */
            if (read_8bit(0x00 + i*0x20,streamFile) != read_8bit(0x02 + i*0x20,streamFile) ||
                read_8bit(0x01 + i*0x20,streamFile) != read_8bit(0x03 + i*0x20,streamFile))
                goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = get_streamfile_size(streamFile) / 32 * 28;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_NGC_DTK;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_NGC_ADPDTK;


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

