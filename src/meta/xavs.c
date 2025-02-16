#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "xavs_streamfile.h"

/* XAVS - Reflections audio and video+audio container [Stuntman (PS2)] */
VGMSTREAM* init_vgmstream_xavs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t start_offset;
    int loop_flag, channels;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00, sf, "XAVS"))
        return NULL;
    if (!check_extensions(sf, "xav"))
        return NULL;

    loop_flag  = 0;
    channels = 2;
    start_offset = 0x00;

    /* 0x04: 16b width + height (0 if file has no video) */
    /* 0x08: related to video (0 if file has no video) */
    total_subsongs = read_u16le(0x0c, sf);
    /* 0x0c: volume? (0x50, 0x4e) */
    /* 0x10: biggest video chunk? (0 if file has no video) */
    /* 0x14: biggest audio chunk? */

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;

    /* could use a blocked layout, but this needs interleaved PCM within blocks which can't be done ATM */
    temp_sf = setup_xavs_streamfile(sf, 0x18, target_subsong - 1);
    if (!temp_sf) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XAVS;
    vgmstream->num_streams = total_subsongs;

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;

    /* no apparent flags, most videos use 0x41 but not all */
    {
        off_t offset = 0x18;
        while (offset < get_streamfile_size(sf)) {
            uint32_t chunk_id   = read_u32le(offset+0x00, sf) & 0xFF;
            uint32_t chunk_size = read_u32le(offset+0x00, sf) >> 8;

            if ((chunk_id & 0xF0) == 0x40) {
                vgmstream->sample_rate = 48000;
                vgmstream->interleave_block_size = 0x200;
                break;
            } else if ((chunk_id & 0xF0) == 0x60) {
                vgmstream->sample_rate = 24000;
                vgmstream->interleave_block_size = 0x100;
                break;
            } else if (chunk_id == 0x56) {
                offset += 0x04 + chunk_size;
            } else if (chunk_id == 0x21) {
                offset += 0x04;
            } else {
                goto fail;
            }
        }
    }

    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(temp_sf), channels, 16);

    if (!vgmstream_open_stream(vgmstream, temp_sf, start_offset))
        goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
