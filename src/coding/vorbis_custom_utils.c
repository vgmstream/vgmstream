#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS

int build_header_comment(uint8_t* buf, int bufsize) {
    int bytes = 0x19;

    if (bytes > bufsize) return 0;

    put_u8   (buf+0x00, 0x03);              /* packet_type (comments) */
    memcpy   (buf+0x01, "vorbis", 6);       /* id */
    put_u32le(buf+0x07, 0x09);              /* vendor_length */
    memcpy   (buf+0x0b, "vgmstream", 9);    /* vendor_string */
    put_u32le(buf+0x14, 0x00);              /* user_comment_list_length */
    put_u8   (buf+0x18, 0x01);              /* framing_flag (fixed) */

    return bytes;
}

int build_header_identification(uint8_t* buf, int bufsize, vorbis_custom_config* cfg) {
    int bytes = 0x1e;

    if (bytes > bufsize)
        return 0;

    // long (bigger frames) + short (smaller frames) blocksizes (samples per frame)
    uint8_t blocksizes = (cfg->blocksize_0_exp << 4) | (cfg->blocksize_1_exp);

    put_u8   (buf+0x00, 0x01);              /* packet_type (id) */
    memcpy   (buf+0x01, "vorbis", 6);       /* id */
    put_u32le(buf+0x07, 0x00);              /* vorbis_version (fixed) */
    put_u8   (buf+0x0b, cfg->channels);     /* audio_channels */
    put_u32le(buf+0x0c, cfg->sample_rate);  /* audio_sample_rate */
    put_u32le(buf+0x10, 0x00);              /* bitrate_maximum (optional hint) */
    put_u32le(buf+0x14, 0x00);              /* bitrate_nominal (optional hint) */
    put_u32le(buf+0x18, 0x00);              /* bitrate_minimum (optional hint) */
    put_u8   (buf+0x1c, blocksizes);        /* blocksize_0 + blocksize_1 nibbles */
    put_u8   (buf+0x1d, 0x01);              /* framing_flag (fixed) */

    return bytes;
}

#if 0
bool make_header_identification(vorbis_custom_codec_data* data, vorbis_custom_config* cfg) {

    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0)
        return false; 
    return true;
}
#endif

// basic log2 for allowed blocksizes (2-exp)
int vorbis_get_blocksize_exp(int blocksize) {
    switch(blocksize) {
        case 64:   return 6;
        case 128:  return 7;
        case 256:  return 8;
        case 512:  return 9;
        case 1024: return 10;
        case 2048: return 11;
        case 4096: return 12;
        case 8192: return 13;
        default:   return 0;
    }
}

bool load_header_packet(STREAMFILE* sf, vorbis_custom_codec_data* data, uint32_t packet_size, int packet_skip, uint32_t* p_offset) {
    if (packet_size > data->buffer_size)
        goto fail;

    data->op.bytes = read_streamfile(data->buffer, *p_offset + packet_skip, packet_size, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0)
        goto fail;
    *p_offset += packet_skip + packet_size;

    return true;
fail:
    return false;
}

#endif
