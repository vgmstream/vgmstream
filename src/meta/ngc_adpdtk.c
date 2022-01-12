#include <math.h>
#include "meta.h"
#include "../util.h"

/* DTK - headerless Nintendo GC DTK file [Harvest Moon: Another Wonderful Life (GC), XGRA (GC)] */
VGMSTREAM* init_vgmstream_dtk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag;


    /* checks */
    /* .dtk: standard [XGRA (GC)]
     * .adp: standard [Harvest Moon AWL (GC)]
     * .wav/lwav: Alien Hominid (GC) */
    if (!check_extensions(sf,"dtk,adp,wav,lwav"))
        goto fail;

    /* check valid frames as files have no header, and .adp/wav are common */
    {
        int i, blanks = 0;
        for (i = 0; i < 10; i++) { /* try a bunch of frames */
            /* header 0x00/01 are repeated in 0x02/03 (for error correction?),
             * could also test header values (upper nibble should be 0..3, and lower nibble 0..C) */
            if (read_u8(0x00 + i*0x20,sf) != read_u8(0x02 + i*0x20,sf) ||
                read_u8(0x01 + i*0x20,sf) != read_u8(0x03 + i*0x20,sf))
                goto fail;

            /* silent frame headers are almost always 0x0C, save a few uncommon tracks [Wave Race Blue Storm (GC), 1080 Silver Storm (GC)] */
            if (read_u16be(0x00 + i*0x20,sf) == 0x00) 
                blanks++;
        }

        if (blanks > 3)
            goto fail;
    }

    /* DTK (Disc Track) are DVD hardware-decoded streams, always stereo and no loop.
     * Some games fake looping by calling DVD commands to set position with certain timing.
     * Though headerless, format is HW-wired to those specs. */
    channels = 2;
    loop_flag = 0;
    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = get_streamfile_size(sf) / 0x20 * 28;
    vgmstream->sample_rate = 48000; /* due to a GC hardware defect this may be closer to 48043 */
    vgmstream->coding_type = coding_NGC_DTK;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DTK;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
