#include <math.h>
#include "meta.h"
#include "../util.h"

/* DTK - headerless Nintendo GC DTK file [Harvest Moon: Another Wonderful Life (GC), XGRA (GC)] */
VGMSTREAM * init_vgmstream_ngc_adpdtk(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag;


    /* checks */
    /* .dtk: standard [XGRA (GC)], .adp: standard [Harvest Moon AWL (GC)], .wav/lwav: Alien Hominid (GC) */
    if ( !check_extensions(streamFile,"dtk,adp,wav,lwav"))
        goto fail;

    /* check valid frames as files have no header, and .adp/wav are common */
    {
        int i;
        for (i = 0; i < 10; i++) { /* try a bunch of frames */
            if (read_8bit(0x00 + i*0x20,streamFile) != read_8bit(0x02 + i*0x20,streamFile) ||
                read_8bit(0x01 + i*0x20,streamFile) != read_8bit(0x03 + i*0x20,streamFile))
                goto fail;
            /* header 0x00/01 are repeated in 0x02/03 (for error correction?),
             * could also test header values (upper nibble should be 0..3, and lower nibble 0..C) */
        }
    }

    /* always stereo, no loop (since it's hardware-decoded and streamed) */
    channel_count = 2;
    loop_flag = 0;
    start_offset = 0x00;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = get_streamfile_size(streamFile) / 0x20 * 28;
    vgmstream->sample_rate = 48000; /* due to a GC hardware defect this may be closer to 48043 */
    vgmstream->coding_type = coding_NGC_DTK;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_NGC_ADPDTK;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
