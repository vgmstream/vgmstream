#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* MUSC - from Krome's PS2 games (The Legend of Spyro, Ty the Tasmanian Tiger) */
VGMSTREAM* init_vgmstream_musc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channels;
    off_t start_offset;
    size_t data_size;


    /* checks */
    /* .mus: actual extension
     * .musc: header ID */
    if (!check_extensions(sf,"mus,musc"))
        goto fail;
    if (!is_id32be(0x00,sf, "MUSC"))
        goto fail;

    start_offset = read_u32le(0x10,sf);
    data_size    = read_u32le(0x14,sf);
    if (start_offset + data_size != get_streamfile_size(sf))
        goto fail;
    /* always does full loops unless it ends in silence */
    loop_flag = read_u32be(get_streamfile_size(sf) - 0x10,sf) != 0x0C000000;
    channels = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_u16le(0x06,sf);
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_MUSC;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_u32le(0x18,sf) / 2;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
