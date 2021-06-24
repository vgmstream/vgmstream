#include "meta.h"
#include "../coding/coding.h"

/* AHV - from Amuze games [Headhunter (PS2)] */
VGMSTREAM* init_vgmstream_ahv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size, channel_size, interleave, sample_rate;
    int loop_flag, channels;


    /* checks */
    /* .ahv: from names in bigfile */
    if (!check_extensions(sf,"ahv"))
        goto fail;
    if (!is_id32be(0x00,sf, "AHV\0"))
        goto fail;

    start_offset = 0x800;
    data_size = get_streamfile_size(sf) - start_offset;

    channel_size = read_u32le(0x08,sf);
    sample_rate = read_32bitLE(0x0c,sf);
    interleave = read_u32le(0x10,sf);
    channels = (interleave != 0) ? 2 : 1;
    loop_flag = 0;
    /* VAGp header after 0x14 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AHV;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    if (interleave)
        vgmstream->interleave_last_block_size = (data_size % (interleave*channels)) / channels;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
