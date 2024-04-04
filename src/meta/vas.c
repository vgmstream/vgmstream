#include "meta.h"
#include "../coding/coding.h"

/* VAS - Manhunt 2 [PSP] */
VGMSTREAM* init_vgmstream_rstm_rockstar(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t stream_offset;
    size_t stream_size;
    int sample_rate, channels, loop_flag = 0;


    /* checks */

    /* VAGs: v1, used in prerelease builds
     * 2AGs: v2, used in the final release */
    if (!is_id32be(0x00, sf, "VAGs") && !is_id32be(0x00, sf, "2AGs"))
        goto fail;

    if (!check_extensions(sf, "vas"))
        goto fail;

    stream_size = read_u32le(0x04, sf);
    sample_rate = read_u16le(0x08, sf);
    /* read_u8(0x0C, sf); // interleave size * 0x10? never used, always mono */
    channels = read_u8(0x0B, sf);
    if (channels != 1) goto fail;

    stream_offset = 0x0C;
    /* offset by 32 bytes for v2, stream name field? always empty in Manhunt 2 */
    if (is_id32be(0x00, sf, "2AGs"))
        stream_offset += 0x20;

    /* might conflict with the standard VAG otherwise */
    if (stream_size + stream_offset != get_streamfile_size(sf))
        goto fail;
    

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAS;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = stream_size;
    vgmstream->interleave_block_size = 0;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
