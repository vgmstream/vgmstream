#include "meta.h"
#include "../coding/coding.h"


/* BCSTM - Nintendo 3DS format */
VGMSTREAM* init_vgmstream_bcstm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    off_t info_offset = 0, seek_offset = 0, data_offset = 0;
    int channels, loop_flag, codec;
    bool is_camelot_ima = false;


    /* checks */
    if (!is_id32be(0x00, sf, "CSTM"))
        return NULL;
    if (!check_extensions(sf,"bcstm"))
        return NULL;

    // 04: BOM
    // 06: header size (0x40)
    // 08: version (0x00000400)
    // 0c: file size (not accurate for Camelot IMA)

    if (read_u16le(0x04, sf) != 0xFEFF)
        return NULL;

    /* get sections (should always appear in the same order though) */
    int section_count = read_u16le(0x10, sf);
    for (int i = 0; i < section_count; i++) {
        // 00: id
        // 02 padding
        // 04: offset
        // 08: size
        uint16_t section_id = read_u16le(0x14 + i * 0x0c + 0x00, sf);
        switch(section_id) {
            case 0x4000: info_offset = read_u32le(0x14 + i * 0x0c + 0x04, sf); break;
            case 0x4001: seek_offset = read_u32le(0x14 + i * 0x0c + 0x04, sf); break;
            case 0x4002: data_offset = read_u32le(0x14 + i * 0x0c + 0x04, sf); break;
            //case 0x4003: regn_offset = read_u32le(0x18 + i * 0x0c + 0x04, sf); break; // ?
            //case 0x4004: pdat_offset = read_u32le(0x18 + i * 0x0c + 0x04, sf); break; // ?
            default:
                break;
        }
    }

    /* INFO section */
    if (!is_id32be(info_offset, sf, "INFO"))
        return NULL;
    codec = read_u8(info_offset + 0x20, sf);
    loop_flag = read_8bit(info_offset + 0x21, sf);
    channels = read_8bit(info_offset + 0x22, sf);


    /* Camelot games have some weird codec hijack [Mario Tennis Open (3DS), Mario Golf: World Tour (3DS)] */
    if (codec == 0x02 && !is_id32be(seek_offset, sf, "SEEK")) {
        if (seek_offset == 0) goto fail;
        start_offset = seek_offset;
        is_camelot_ima = true;
    }
    else {
        if (data_offset == 0) goto fail;
        start_offset = data_offset + 0x20;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(info_offset + 0x24, sf);
    vgmstream->num_samples = read_s32le(info_offset + 0x2c, sf);
    vgmstream->loop_start_sample = read_s32le(info_offset + 0x28, sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_CSTM;
    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_u32le(info_offset + 0x34, sf);
    vgmstream->interleave_last_block_size = read_u32le(info_offset + 0x44, sf);

    /* Camelot doesn't follow header values */
    if (is_camelot_ima) {
        size_t data_size = get_streamfile_size(sf) - start_offset;
        vgmstream->interleave_block_size = 0x200;
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size * channels)) / channels;
    }

    switch(codec) {
        case 0x00:
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x01:
            vgmstream->coding_type = coding_PCM16LE;
            break;
        case 0x02:
            vgmstream->coding_type = coding_NGC_DSP;

            if (is_camelot_ima) {
                vgmstream->coding_type = coding_CAMELOT_IMA;
            }
            else {
                off_t channel_indexes, channel_info_offset, coefs_offset;

                channel_indexes = info_offset+0x08 + read_u32le(info_offset + 0x1C, sf);
                for (int i = 0; i < vgmstream->channels; i++) {
                    channel_info_offset = channel_indexes + read_u32le(channel_indexes + 0x04 + (i * 0x08) + 0x04, sf);
                    coefs_offset = channel_info_offset + read_u32le(channel_info_offset + 0x04, sf);

                    for (int c = 0; c < 16; c++) {
                        vgmstream->ch[i].adpcm_coef[c] = read_s16le(coefs_offset + c * 0x02, sf);
                    }
                }
            }
            break;

        default: /* 0x03: regular IMA? (like .bcwav) */
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
