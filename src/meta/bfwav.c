#include "meta.h"
#include "../coding/coding.h"

/* FWAV - Nintendo streams */
VGMSTREAM * init_vgmstream_bfwav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    off_t info_offset, data_offset;
    int channel_count, loop_flag, codec;
    int big_endian;
    size_t interleave = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int nsmbu_flag = 0;


    /* checks */
    /* .bfwavnsmbu: fake extension to detect New Super Mario Bros U files with weird sample rate */
    if (!check_extensions(streamFile, "bfwav,fwav,bfwavnsmbu"))
        goto fail;
    nsmbu_flag = check_extensions(streamFile, "bfwavnsmbu");

    /* FWAV header */
    if (read_32bitBE(0x00, streamFile) != 0x46574156) /* "FWAV" */
        goto fail;
    /* 0x06(2): header size (0x40), 0x08: version (0x00010200), 0x0c: file size 0x10(2): sections (2) */

    if ((uint16_t)read_16bitBE(0x04, streamFile) == 0xFEFF) { /* BE BOM check */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    } else if ((uint16_t)read_16bitBE(0x04, streamFile) == 0xFFFE) { /* LE BOM check */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        big_endian = 0;
    } else {
        goto fail;
    }

    info_offset = read_32bit(0x18, streamFile); /* 0x14(2): info mark (0x7000), 0x1c: info size */
    data_offset = read_32bit(0x24, streamFile); /* 0x20(2): data mark (0x7001), 0x28: data size */

    /* INFO section */
    if (read_32bitBE(info_offset, streamFile) != 0x494E464F)  /* "INFO" */
        goto fail;
    codec = read_8bit(info_offset + 0x08, streamFile);
    loop_flag = read_8bit(info_offset + 0x09, streamFile);
    channel_count = read_32bit(info_offset + 0x1C, streamFile);

    /* parse channel table */
    {
        off_t channel1_info, data_start;
        int i;

        channel1_info = info_offset + 0x1c + read_32bit(info_offset+0x20+0x08*0+0x04, streamFile);
        data_start = read_32bit(channel1_info+0x04, streamFile); /* within "DATA" after 0x08 */

        /* channels use absolute offsets but should be ok as interleave */
        interleave = 0;
        if (channel_count > 1) {
            off_t channel2_info = info_offset + 0x1c + read_32bit(info_offset+0x20+0x08*1+0x04, streamFile);
            interleave = read_32bit(channel2_info+0x04, streamFile) - data_start;
        }

        start_offset = data_offset + 0x08 + data_start;

        /* validate all channels just in case of multichannel with non-constant interleave */
        for (i = 0; i < channel_count; i++) {
            /* channel table, 0x00: flag (0x7100), 0x04: channel info offset */
            off_t channel_info = info_offset + 0x1c + read_32bit(info_offset+0x20+0x08*i+0x04, streamFile);
            /* channel info, 0x00(2): flag (0x1f00), 0x04: offset, 0x08(2): ADPCM flag (0x0300), 0x0c: ADPCM offset */
            if ((uint16_t)read_16bit(channel_info+0x00, streamFile) != 0x1F00)
                goto fail;
            if (read_32bit(channel_info+0x04, streamFile) != data_start + interleave*i)
                goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bit(info_offset + 0x0C, streamFile);
    if (nsmbu_flag)
        vgmstream->sample_rate = 16000;

    vgmstream->num_samples = read_32bit(info_offset + 0x14, streamFile);
    vgmstream->loop_start_sample = read_32bit(info_offset + 0x10, streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_FWAV;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
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
                        coef_header = info_offset + 0x1C + read_32bit(info_offset + 0x24 + (i*0x08), streamFile);
                        coef_offset = read_32bit(coef_header + 0x0c, streamFile) + coef_header;
                        vgmstream->ch[i].adpcm_coef[c] = read_16bit(coef_offset + c*2, streamFile);
                    }
                }
            }
            break;

        default: /* 0x03: IMA? */
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
