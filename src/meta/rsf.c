#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .rsf - from Metroid Prime */

VGMSTREAM* init_vgmstream_rsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    uint32_t interleave, file_size;

    /* checks */
    if (!check_extensions(sf,"rsf"))
        goto fail;

    /* this is all we have to go on, rsf is completely headerless */

    file_size = get_streamfile_size(sf);
    interleave = (file_size + 1) / 2;

    /* G.721 has no zero nibbles, so we look at the first few bytes 
     * (known files start with 0xFFFFFFFF, but probably an oddity of the codec) */
    {
        uint8_t test_byte;
        off_t i;

        /* 0x20 is arbitrary, all files are much larger */
        for (i = 0; i < 0x20; i++) {
            test_byte = read_u8(i,sf);
            if (!(test_byte&0xf) || !(test_byte&0xf0)) goto fail;
        }

        /* and also check start of second channel */
        for (i = interleave; i < interleave + 0x20; i++) {
            test_byte = read_u8(i,sf);
            if (!(test_byte&0xf) || !(test_byte&0xf0)) goto fail;
        }
    }

    channels = 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = file_size;
    vgmstream->sample_rate = 32000;

    vgmstream->coding_type = coding_G721;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RSF;

    if (!vgmstream_open_stream(vgmstream, sf, 0))
        goto fail;

    /* open the file for reading by each channel */
    {
        int i;
        for (i = 0; i < channels; i++) {
            vgmstream->ch[i].channel_start_offset= vgmstream->ch[i].offset = interleave * i;
            g72x_init_state(&(vgmstream->ch[i].g72x_state));
        }
    }

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
