#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"

/* EZW - from AmuseWorld games [EZ2DJ 5TH (AC)] */
VGMSTREAM* init_vgmstream_ezw(STREAMFILE* sf) {

    /* checks */
    int channels = read_s16le(0x00,sf);
    if (channels < 1 || channels > 16) //arbitrary max
        return NULL;

    // .ezw: EZ2DJ
    // .ssf: EZ2AC    
    if (!check_extensions(sf,"ezw,ssf"))
        return NULL;

    // no header ID but internally it's referred as the "EZW Format"
    // (some early games use regular .wav instead)

    meta_header_t h = {0};
    h.meta = meta_EZW;

    h.channels      = read_s16le(0x00, sf);
    h.sample_rate   = read_s32le(0x02, sf);
    // 06: bitrate
    h.interleave    = read_s16le(0x0A, sf) / channels;
    int bps         = read_s16le(0x0C, sf);
    h.stream_size   = read_u32le(0x0E,sf);

    if (h.interleave != 0x02)
        return NULL;
    if (bps != 16)
        return NULL;

    h.stream_offset = 0x12;
    h.num_samples   = pcm16_bytes_to_samples(h.stream_size, channels);

    h.coding = coding_PCM16LE;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
