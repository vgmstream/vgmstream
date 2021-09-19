#include "meta.h"
#include "../coding/coding.h"

/* FWAV - Nintendo streams */
VGMSTREAM* init_vgmstream_bfwav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    off_t info_offset, data_offset;
    int channels, loop_flag, codec, sample_rate;
    int big_endian;
    size_t interleave = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "FWAV"))
        goto fail;

    /* .bfwavnsmbu: fake extension to detect New Super Mario Bros U files with weird sample rate */
    if (!check_extensions(sf, "bfwav,fwav,bfwavnsmbu"))
        goto fail;

    /* BOM check */
    if (read_u16be(0x04, sf) == 0xFEFF) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    } else if (read_u16be(0x04, sf) == 0xFFFE) {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        big_endian = 0;
    } else {
        goto fail;
    }

    /* FWAV header */
    /* 0x06(2): header size (0x40) */
    /* 0x08: version (0x00010200) */
    /* 0x0c: file size */
    /* 0x10(2): sections (2) */

    /* 0x14(2): info mark (0x7000) */
    info_offset = read_32bit(0x18, sf);
    /* 0x1c: info size */

    /* 0x20(2): data mark (0x7001) */
    data_offset = read_32bit(0x24, sf);
    /* 0x28: data size */

    /* INFO section */
    if (!is_id32be(info_offset, sf, "INFO"))
        goto fail;
    codec = read_u8(info_offset + 0x08, sf);
    loop_flag = read_u8(info_offset + 0x09, sf);
    sample_rate = read_32bit(info_offset + 0x0C, sf);
    channels = read_32bit(info_offset + 0x1C, sf);

    //TODO remove
    if (check_extensions(sf, "bfwavnsmbu"))
        sample_rate = 16000;

    /* parse channel table */
    {
        off_t channel1_info, data_start;
        int i;

        channel1_info = info_offset + 0x1c + read_32bit(info_offset+0x20+0x08*0+0x04, sf);
        data_start = read_32bit(channel1_info+0x04, sf); /* within "DATA" after 0x08 */

        /* channels use absolute offsets but should be ok as interleave */
        interleave = 0;
        if (channels > 1) {
            off_t channel2_info = info_offset + 0x1c + read_32bit(info_offset+0x20+0x08*1+0x04, sf);
            interleave = read_32bit(channel2_info+0x04, sf) - data_start;
        }

        start_offset = data_offset + 0x08 + data_start;

        /* validate all channels just in case of multichannel with non-constant interleave */
        for (i = 0; i < channels; i++) {
            /* channel table, 0x00: flag (0x7100), 0x04: channel info offset */
            off_t channel_info = info_offset + 0x1c + read_32bit(info_offset+0x20+0x08*i+0x04, sf);
            /* channel info, 0x00(2): flag (0x1f00), 0x04: offset, 0x08(2): ADPCM flag (0x0300), 0x0c: ADPCM offset */
            if ((uint16_t)read_16bit(channel_info+0x00, sf) != 0x1F00)
                goto fail;
            if (read_32bit(channel_info+0x04, sf) != data_start + interleave*i)
                goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = read_32bit(info_offset + 0x14, sf);
    vgmstream->loop_start_sample = read_32bit(info_offset + 0x10, sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_FWAV;
    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = interleave;

    switch (codec) {
        case 0x00:
            vgmstream->coding_type = coding_PCM8;
            break;

        case 0x01:
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            break;

        case 0x02:
            vgmstream->coding_type = coding_NGC_DSP;
            {
                int i, c;
                off_t coef_header, coef_offset;

                for (i = 0; i < vgmstream->channels; i++) {
                    for (c = 0; c < 16; c++) {
                        coef_header = info_offset + 0x1C + read_32bit(info_offset + 0x24 + (i*0x08), sf);
                        coef_offset = read_32bit(coef_header + 0x0c, sf) + coef_header;
                        vgmstream->ch[i].adpcm_coef[c] = read_16bit(coef_offset + c*2, sf);
                    }
                }
            }
            break;

        default: /* 0x03: IMA? */
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
