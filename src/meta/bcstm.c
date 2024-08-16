#include "meta.h"
#include "../coding/coding.h"


/* BCSTM - Nintendo 3DS format */
VGMSTREAM * init_vgmstream_bcstm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    off_t info_offset = 0, seek_offset = 0, data_offset = 0;
    int channel_count, loop_flag, codec;
    int is_camelot_ima = 0;


    /* checks */
    if ( !check_extensions(streamFile,"bcstm") )
        goto fail;

    /* CSTM header */
    if (read_32bitBE(0x00, streamFile) != 0x4353544D) /* "CSTM" */
        goto fail;
    /* 0x06(2): header size (0x40), 0x08: version (0x00000400), 0x0c: file size (not accurate for Camelot IMA) */

    /* check BOM */
    if ((uint16_t)read_16bitLE(0x04, streamFile) != 0xFEFF)
        goto fail;

    /* get sections (should always appear in the same order) */
    {
        int i;
        int section_count = read_16bitLE(0x10, streamFile);
        for (i = 0; i < section_count; i++) {
            /* 0x00: id, 0x02(2): padding, 0x04(4): offset, 0x08(4): size */
            uint16_t section_id = read_16bitLE(0x14 + i*0x0c+0x00, streamFile);
            switch(section_id) {
                case 0x4000: info_offset = read_32bitLE(0x14 + i*0x0c+0x04, streamFile); break;
                case 0x4001: seek_offset = read_32bitLE(0x14 + i*0x0c+0x04, streamFile); break;
                case 0x4002: data_offset = read_32bitLE(0x14 + i*0x0c+0x04, streamFile); break;
              //case 0x4003: /* off_t regn_offset = read_32bitLE(0x18 + i * 0xc, streamFile); */ /* ? */
              //case 0x4004: /* off_t pdat_offset = read_32bitLE(0x18 + i * 0xc, streamFile); */ /* ? */
                default:
                    break;
            }
        }
    }

    /* INFO section */
    if (read_32bitBE(info_offset, streamFile) != 0x494E464F) /* "INFO" */
        goto fail;
    codec = read_8bit(info_offset + 0x20, streamFile);
    loop_flag = read_8bit(info_offset + 0x21, streamFile);
    channel_count = read_8bit(info_offset + 0x22, streamFile);


    /* Camelot games have some weird codec hijack [Mario Tennis Open (3DS), Mario Golf: World Tour (3DS)] */
    if (codec == 0x02 && read_32bitBE(seek_offset, streamFile) != 0x5345454B) { /* "SEEK" */
        if (seek_offset == 0) goto fail;
        start_offset = seek_offset;
        is_camelot_ima = 1;
    }
    else {
        if (data_offset == 0) goto fail;
        start_offset = data_offset + 0x20;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(info_offset + 0x24, streamFile);
    vgmstream->num_samples = read_32bitLE(info_offset + 0x2c, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(info_offset + 0x28, streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_CSTM;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(info_offset + 0x34, streamFile);
    vgmstream->interleave_last_block_size = read_32bitLE(info_offset + 0x44, streamFile);

    /* Camelot doesn't follow header values */
    if (is_camelot_ima) {
        size_t blocks_to_subtract;
        size_t samples_per_byte = 2;
        size_t header_block_count = read_32bitLE(info_offset + 0x30, streamFile);
        size_t header_block_size = read_32bitLE(info_offset + 0x34, streamFile);
        size_t header_last_block_size = read_32bitLE(info_offset + 0x44, streamFile);

        size_t data_size_file = get_streamfile_size(streamFile) - start_offset;
        size_t sample_count_header = read_32bitLE(info_offset + 0x2C, streamFile);
        size_t badblock_sample_diff = (data_size_file * samples_per_byte / channel_count) - sample_count_header;

        /* Camelot uses a hardcoded block size of 0x200, rather than the one provided in the header...kind of? */
        vgmstream->interleave_block_size = 0x200;
        vgmstream->interleave_last_block_size = header_last_block_size % vgmstream->interleave_block_size;

        /**
         * The "final" block (or rather, 0x200 bytes before the final block if the header values were actually followed)
         * seems to contain a few bytes of unused(?) data that shifts all of the remaining blocks forward a bit.
         * This leads to issues with data alignment, and corrupts the interleaved audio data as a result.
         * The following parameters are needed to adequately correct for this behavior.
         */
        blocks_to_subtract = 1 - ((header_last_block_size + vgmstream->interleave_block_size) / header_block_size);
        vgmstream->broken_interleave_sample_pos = (((header_block_count - blocks_to_subtract) * header_block_size) - vgmstream->interleave_block_size) * samples_per_byte;
        vgmstream->broken_interleave_sample_count = badblock_sample_diff;
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
                vgmstream->coding_type = coding_NW_IMA;
            }
            else {
                int i, c;
                off_t channel_indexes, channel_info_offset, coefs_offset;

                channel_indexes = info_offset+0x08 + read_32bitLE(info_offset + 0x1C, streamFile);
                for (i = 0; i < vgmstream->channels; i++) {
                    channel_info_offset = channel_indexes + read_32bitLE(channel_indexes+0x04+(i*0x08)+0x04, streamFile);
                    coefs_offset = channel_info_offset + read_32bitLE(channel_info_offset+0x04, streamFile);

                    for (c = 0; c < 16; c++) {
                        vgmstream->ch[i].adpcm_coef[c] = read_16bitLE(coefs_offset + c*2, streamFile);
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
