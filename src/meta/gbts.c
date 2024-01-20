#include "meta.h"
#include "../coding/coding.h"


/* GbTs - from Konami/KCE Studio games [Pop'n Music 9/10 (PS2)] */
VGMSTREAM* init_vgmstream_gbts(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t data_offset, data_size;
    int loop_flag, channels, sample_rate;
    uint32_t loop_start, loop_end;


    /* checks */
    if (!is_id32be(0x00,sf, "GbTs"))
        return NULL;
    /* .gbts: header id (no apparent exts) */
    if (!check_extensions(sf, "gbts"))
        return NULL;


    /* 04: always 0x24 */
    data_offset = read_u32le(0x08,sf);
    data_size   = read_u32le(0x0C,sf); /* without padding */
    loop_start  = read_u32le(0x10,sf); /* (0x20 = start frame if not set) */
    loop_end    = read_u32le(0x14,sf); /* (0x00 if not set) */
    sample_rate = read_s32le(0x18,sf);
    channels    = read_s32le(0x1C,sf);
    /* 20: 1? */
    /* 24: block size (interleave * channels) */
    /* 30+: empty */

    loop_flag = (loop_end > 0);
    loop_end += loop_start; /* loop region matches PS-ADPCM flags */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    vgmstream->meta_type = meta_GBTS;

    if (!vgmstream_open_stream(vgmstream, sf, data_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
