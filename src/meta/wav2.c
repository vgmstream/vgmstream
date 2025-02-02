#include "meta.h"
#include "../coding/coding.h"

/* WAV2 - from Infogrames North America(?) games [Slave Zero (PC), Outcast (PC)] */
VGMSTREAM* init_vgmstream_wav2(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!is_id32be(0x00, sf, "WAV2"))
        return NULL;
    if (!check_extensions(sf,"wv2"))
        return NULL;


    // 04: offset?
    // 08: pcm samples?
    channels = read_u16le(0x0c,sf);
    // 0e: bps
    sample_rate = read_s32le(0x10,sf);
    // 14: average bitrate?
    data_size = read_u32le(0x18, sf);
    loop_flag = 0;
    start_offset = 0x1c;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WAV2;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ima_bytes_to_samples(data_size, channels); /* also 0x18 */

    vgmstream->coding_type = coding_DVI_IMA_mono;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0xFA;
    vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size * channels)) / channels;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
