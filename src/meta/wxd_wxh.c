#include "meta.h"
#include "../coding/coding.h"

/* WXD+WXH - stream container from Relic's earlier games [Homeworld (PC), Homeworld Cataclysm (PC)] */
VGMSTREAM* init_vgmstream_wxd_wxh(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sh = NULL;
    uint32_t start_offset, offset, data_size;
    int loop_flag, channels, bitrate, internal_rate;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "wxd"))
        goto fail;
    if (!is_id32be(0x00,sf, "WXD1"))
        goto fail;
    /* 0x04: crc? */

    /* companion .wxh found in .big bigfiles, must extract and put together */
    sh = open_streamfile_by_ext(sf,"wxh");
    if (!sh) goto fail;

    if (!is_id32be(0x00,sh, "WXH1"))
        goto fail;
    /* 0x04: crc? */

    total_subsongs = read_u32le(0x08,sh);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    offset = 0x0c + (target_subsong-1) * 0x0c;
    start_offset    = read_u32le(offset + 0x00,sh);
    loop_flag       = read_u16le(offset + 0x04,sh);
    bitrate         = read_u16le(offset + 0x06,sh);
    /* 0x08: volume (255=max) */
    channels        = read_u8   (offset + 0x09,sh);
    /* 0x0a: unused (-1) */
    internal_rate   = 44100;

    /* stream info */
    if (!is_id32be(start_offset + 0x00,sf, "DATA"))
        goto fail;
    data_size       = read_u32le(start_offset + 0x04,sf);
    start_offset += 0x08;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WXD_WXH;
    vgmstream->sample_rate = 44100;
    
    vgmstream->num_samples = relic_bytes_to_samples(data_size, channels, bitrate);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;
    
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = data_size;

    vgmstream->codec_data = init_relic(channels, bitrate, internal_rate);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_RELIC;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    close_streamfile(sh);
    return vgmstream;

fail:
    close_streamfile(sh);
    close_vgmstream(vgmstream);
    return NULL;
}
