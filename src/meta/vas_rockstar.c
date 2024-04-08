#include "meta.h"
#include "../coding/coding.h"

/* VAS - Manhunt 2 [PSP] blocked audio format */
VGMSTREAM* init_vgmstream_vas_rockstar(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t stream_offset;
    size_t data_size, stream_size, block_size = 0x40;
    int sample_rate, num_streams, channels, loop_flag = 0;
    int is_v2, target_subsong = sf->stream_index;


    /* checks */
    /* VAGs: v1, used in prerelease builds
     * 2AGs: v2, used in the final release */
    if (!is_id32be(0x00, sf, "VAGs") && !is_id32be(0x00, sf, "2AGs"))
        return NULL;

    if (!check_extensions(sf, "vas"))
        return NULL;


    /* parse header */
    data_size = read_u32le(0x04, sf);
    sample_rate = read_u16le(0x08, sf);
    if (read_u8(0x0A, sf)) goto fail; /* always 0? */
    num_streams = read_u8(0x0B, sf);

    if (target_subsong < 0 || num_streams < 1 || num_streams > 32) goto fail;
    if (!target_subsong) target_subsong = 1;

    channels = 1; /* might be read_u8(0x0A, sf) + 1? */


    /* set up stream */
    is_v2 = read_u8(0x00, sf) == 0x32; /* 2AGs */

    stream_offset = 0x0C;
    /* only in v2, 32 byte buffer of the intended order for stream blocks(?) */
    /* always 00 01 02 03 04 05 06 in the multi-stream music/ambience files */
    if (is_v2) stream_offset += 0x20;

    /* might conflict with the standard VAG otherwise */
    if (data_size + stream_offset != get_streamfile_size(sf))
        goto fail;

    target_subsong -= 1; /* zero index */
    /* currently threre are no known v1 multi-stream blocked sounds, prerelease
     * builds also use v2 for those, but this should be how v1 works in theory */
    stream_offset += block_size * (is_v2 ? read_u8(0x0C + target_subsong, sf) : target_subsong);

    stream_size = data_size / num_streams;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAS_ROCKSTAR;
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_streams = num_streams;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = stream_size;
    vgmstream->interleave_block_size = 0;
    vgmstream->layout_type = layout_blocked_vas;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
