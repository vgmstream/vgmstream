#include "meta.h"
#include "../coding/coding.h"

/* WXD+WXH - stream container from Relic's earlier games [Homeworld (PC), Homeworld Cataclysm (PC)] */
VGMSTREAM* init_vgmstream_wxh_wxd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    uint32_t start_offset, offset, data_size;
    int loop_flag, channels, bitrate, internal_rate;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "WXH1"))
        return NULL;
    if (!check_extensions(sf, "wxh"))
        return NULL;
    //04: crc?

    total_subsongs = read_u32le(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    offset = 0x0c + (target_subsong-1) * 0x0c;
    start_offset    = read_u32le(offset + 0x00,sf);
    loop_flag       = read_u16le(offset + 0x04,sf);
    bitrate         = read_u16le(offset + 0x06,sf);
    //08: volume (255=max)
    channels        = read_u8   (offset + 0x09,sf);
    //0a: unused (-1)
    internal_rate   = 44100;


    /* .wxh found in .big bigfiles, must extract and put together with .wxh */
    sb = open_streamfile_by_ext(sf,"wxd");
    if (!sb) goto fail;

    if (!is_id32be(0x00,sb, "WXD1"))
        goto fail;
    /* 0x04: crc? */

    /* stream info */
    if (!is_id32be(start_offset + 0x00,sb, "DATA"))
        goto fail;
    data_size       = read_u32le(start_offset + 0x04,sb);
    start_offset += 0x08;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WXH_WXD;
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

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sf);
    return vgmstream;

fail:
    close_streamfile(sf);
    close_vgmstream(vgmstream);
    return NULL;
}
