#include "meta.h"
#include "../coding/coding.h"


/* .VOI - from Raw Danger (PS2) */
VGMSTREAM* init_vgmstream_voi(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag = 0;
    off_t start_offset;


    /* checks */
    if (read_u32le(0x00,sf) != 1 && read_u32le(0x00,sf) != 2)
        return NULL;

    if (!check_extensions(sf, "voi"))
        return NULL;

    /* probably number of samples of all channels */
    if ((read_u32le(0x04,sf) * 2 + 0x800) != get_streamfile_size(sf))
        return NULL;

    channels = read_s32le(0x00,sf);
    loop_flag = 0;
    start_offset = 0x800;
    

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VOI;
    vgmstream->num_samples = pcm16_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;

    if (read_32bitLE(0x08,sf) == 0) {
        vgmstream->sample_rate = 48000;
        vgmstream->interleave_block_size = 0x200;
    }
    else if (read_32bitLE(0x08,sf) == 1) {
        vgmstream->sample_rate = 24000;
        vgmstream->interleave_block_size = 0x100;
    }
    else {
        goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
