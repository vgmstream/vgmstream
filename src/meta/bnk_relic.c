#include "meta.h"
#include "../coding/coding.h"

/* BNK - sfx container from Relic's earlier games [Homeworld (PC), Homeworld Cataclysm (PC)] */
VGMSTREAM* init_vgmstream_bnk_relic(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, offset, data_size, loop_start, loop_end;
    int loop_flag, channels, bitrate, internal_rate, sample_rate;
    int total_subsongs, target_subsong = sf->stream_index;
    

    /* checks */
    if (!check_extensions(sf, "bnk"))
        goto fail;
    if (!is_id32be(0x00,sf, "BNK0"))
        goto fail;
    /* 0x04: flags? */

    total_subsongs = read_u32le(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    offset = 0x0c + (target_subsong-1) * 0x38;
    if (!is_id32be(offset + 0x00,sf, "PCH0"))
        goto fail;
    /* 0x04: null */
    /* 0x08: 0/+-number? */
    start_offset    = read_u32le(offset + 0x0c,sf);
    data_size       = read_u32le(offset + 0x10,sf);
    loop_start = read_u32le(offset + 0x14,sf); /* 0x14: 0/offset? */
    loop_end = read_u32le(offset + 0x18,sf); /* 0x18: 0/offset? */
    bitrate         = read_u16le(offset + 0x1c,sf);
    loop_flag       = read_u16le(offset + 0x1e,sf);
    /* 0x20: volume? */
    /* 0x22: -1 */
    /* 0x24: fmt pcm codec 1 */
    channels        = read_u16le(offset + 0x26,sf);
    sample_rate     = read_u32le(offset + 0x28,sf);
    /* 0x2c: bitrate */
    /* 0x30: pcm block size */
    /* 0x32: pcm bps */
    /* 0x34: 0 */
    /* 0x36: -1 */

    internal_rate = 44100;

    loop_flag = 0; //todo clicks on loop, wrong calcs?

    /* stream info */
    if (!is_id32be(start_offset - 0x04,sf, "DATA"))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BNK_RELIC;
    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = relic_bytes_to_samples(data_size, channels, bitrate);
    vgmstream->loop_start_sample = relic_bytes_to_samples(loop_start, channels, bitrate);
    vgmstream->loop_end_sample = relic_bytes_to_samples(loop_end, channels, bitrate);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = data_size;

    vgmstream->codec_data = init_relic(channels, bitrate, internal_rate);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_RELIC;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
