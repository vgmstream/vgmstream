#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* FILp - from Resident Evil: Dead Aim (PS2) */
VGMSTREAM* init_vgmstream_filp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag;


    /* checks */
    if (!is_id32be(0x00,sf, "FILp"))
        return NULL;
    /* .fil: extension in bigfile */
    if (!check_extensions(sf,"fil"))
        return NULL;

    channels = read_s32le(0x04,sf); /* stereo only though */
    if (read_32bitLE(0x0C,sf) != get_streamfile_size(sf))
        goto fail;
    loop_flag = (read_u32le(0x34,sf) == 0x00); /* 00/01/02 */

    if (!is_id32be(0x100,sf, "VAGp"))
        return NULL;
    if (!is_id32be(0x130,sf, "VAGp"))
        return NULL;

    start_offset = 0x00; /* multiple FILp blocks pasted together (each including VAGps, but their sizes refer to the whole thing) */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FILP;
    vgmstream->sample_rate = read_s32le(0x110,sf);

    vgmstream->num_samples = ps_bytes_to_samples(read_u32le(0x10C,sf), 1); /* channel size for the whole stream */
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_filp;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    block_update(start_offset, vgmstream);

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
