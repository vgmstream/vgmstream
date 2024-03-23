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

bool make_header_identification(vorbis_custom_codec_data* data, vorbis_custom_config* cfg) {

    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0)
        return false; 
    return true;
}

void load_blocksizes(vorbis_custom_config* cfg, int blocksize_short, int blocksize_long) {
    uint8_t exp_blocksize_0, exp_blocksize_1;

    /* guetto log2 for allowed blocksizes (2-exp), could be improved */
    switch(blocksize_long) {
        case 64:   exp_blocksize_0 = 6;  break;
        case 128:  exp_blocksize_0 = 7;  break;
        case 256:  exp_blocksize_0 = 8;  break;
        case 512:  exp_blocksize_0 = 9;  break;
        case 1024: exp_blocksize_0 = 10; break;
        case 2048: exp_blocksize_0 = 11; break;
        case 4096: exp_blocksize_0 = 12; break;
        case 8192: exp_blocksize_0 = 13; break;
        default:   exp_blocksize_0 = 0;
    }
    switch(blocksize_short) {
        case 64:   exp_blocksize_1 = 6;  break;
        case 128:  exp_blocksize_1 = 7;  break;
        case 256:  exp_blocksize_1 = 8;  break;
        case 512:  exp_blocksize_1 = 9;  break;
        case 1024: exp_blocksize_1 = 10; break;
        case 2048: exp_blocksize_1 = 11; break;
        case 4096: exp_blocksize_1 = 12; break;
        case 8192: exp_blocksize_1 = 13; break;
        default:   exp_blocksize_1 = 0;
    }

    cfg->blocksize_0_exp = exp_blocksize_0;
    cfg->blocksize_1_exp = exp_blocksize_1;
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
#endif/* VGM_USE_VORBIS */