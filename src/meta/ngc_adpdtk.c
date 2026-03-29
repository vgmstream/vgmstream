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
     * .adp: standard [Harvest Moon: AWL (GC)]
     * .trk: Bloody Roar: Primal Fury (GC)
     * .wav/lwav: Alien Hominid (GC) */
    if (!check_extensions(sf,"dtk,adp,trk,wav,lwav"))
        return NULL;

    /* check valid frames as files have no header, and .adp/wav are common */
    {
        int max_frames = 10; //arbitrary max
        int blanks = 0;
        uint32_t offset = 0x00;
        for (int i = 0; i < max_frames; i++) {
            // upper nibble should be 0..3, and lower nibble 0..C
            uint8_t frame_header = read_u8(offset + 0x00,sf);
            int index = (frame_header >> 4) & 0xf;
            int shift = (frame_header >> 0) & 0xf;
            if(index > 4 || shift > 0x0c)
                return NULL;

            // header 0x00/01 are repeated in 0x02/03 (for error correction?)
            if (read_u8(offset + 0x00,sf) != read_u8(offset + 0x02,sf) ||
                read_u8(offset + 0x01,sf) != read_u8(offset + 0x03,sf))
                return NULL;

            // silent frame headers are almost always 0x0C, save a few uncommon tracks [Wave Race Blue Storm (GC), 1080 Silver Storm (GC)]
            if (read_u16be(offset + 0x00,sf) == 0x00) 
                blanks++;
            offset += 0x20;
        }

        if (blanks > 3)
            return NULL;
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
    vgmstream->sample_rate = 48000; // due to a GC hardware defect this may be closer to 48043
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
