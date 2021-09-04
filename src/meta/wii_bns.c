#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* BNS - Wii "Banner Sound" disc jingle */
VGMSTREAM* init_vgmstream_wii_bns(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t bns_offset;
    uint32_t info_offset = 0, data_offset = 0;
    uint32_t channel_info_offset_list_offset;
    int channels, loop_flag, sample_rate;
    uint32_t sample_count, loop_start;

    /* checks */
    /* .bin: actual extension
     * .bns: header id */
    if (!check_extensions(sf, "bin,lbin,bns"))
        goto fail;

    bns_offset = 0;
    if (is_id32be(bns_offset + 0x40, sf, "IMET")) {
        /* regular .bnr, find sound.bin offset */
        bns_offset = read_u32be(bns_offset + 0x44,sf);

        /* tables, probably not ok for all cases */
        bns_offset += read_u32be(bns_offset + 0x54,sf);
    }

    if (is_id32be(bns_offset + 0x00, sf, "IMD5")) {
        /* skip IMD5 header if present */
        bns_offset += 0x20;
    }

    if (!is_id32be(bns_offset + 0x00,sf, "BNS "))
        goto fail;
    if (read_u32be(bns_offset + 0x04,sf) != 0xFEFF0100u)
        goto fail;

    /* find chunks */
    {
        uint32_t file_size   = read_u32be(bns_offset+0x08,sf);
        uint32_t header_size = read_u16be(bns_offset+0x0c,sf);
        uint16_t chunk_count = read_u16be(bns_offset+0x0e,sf);
        int i;

        /* assume BNS is the last thing in the file */
        if (file_size + bns_offset != get_streamfile_size(sf))
            goto fail;

        for (i = 0; i < chunk_count; i++) {
            uint32_t chunk_info_offset = bns_offset+0x10+i*0x08;
            uint32_t chunk_offset, chunk_size;

            // ensure chunk info is within header
            if (chunk_info_offset+8 > bns_offset+header_size) goto fail;

            chunk_offset = bns_offset + (uint32_t)read_32bitBE(chunk_info_offset,sf);
            chunk_size = (uint32_t)read_32bitBE(chunk_info_offset+4,sf);

            // ensure chunk is within file
            if (chunk_offset < bns_offset+header_size ||
                chunk_offset+chunk_size > bns_offset+file_size) goto fail;

            // ensure chunk size in header matches that listed in chunk
            // Note: disabled for now, as the Homebrew Channel BNS has a DATA
            // chunk that doesn't include the header size
            //if ((uint32_t)read_32bitBE(chunk_offset+4,sf) != chunk_size) goto fail;

            // handle each chunk type
            switch (read_32bitBE(chunk_offset,sf)) {
                case 0x494E464F:    // INFO
                    info_offset = chunk_offset+0x08;
                    break;
                case 0x44415441:    // DATA
                    data_offset = chunk_offset+0x08;
                    break;
                default:
                    goto fail;
            }
        }

        /* need both INFO and DATA */
        if (!info_offset || !data_offset) goto fail;
    }

    /* parse out basic stuff in INFO */
    {
        /* only seen this zero, specifies DSP format? */
        if (read_8bit(info_offset+0x00,sf) != 0) goto fail;

        loop_flag = read_8bit(info_offset+0x01,sf);
        channels = read_8bit(info_offset+0x02,sf);

        /* only seen zero, padding? */
        if (read_8bit(info_offset+0x03,sf) != 0) goto fail;

        sample_rate = read_u16be(info_offset+0x04,sf);

        /* only seen this zero, padding? */
        if (read_u16be(info_offset+0x06,sf) != 0) goto fail;

        loop_start = read_s32be(info_offset+0x08,sf);

        sample_count = read_s32be(info_offset+0x0c,sf);

        channel_info_offset_list_offset = info_offset + read_u32be(info_offset+0x10,sf);
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WII_BNS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = sample_count;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    //todo cleanup
    /* open the file for reading */
    {
        int i, j;
        for (i = 0; i < channels; i++) {
            uint32_t channel_info_offset = info_offset + read_u32be(channel_info_offset_list_offset + 0x04*i,sf);
            uint32_t channel_data_offset = data_offset + read_u32be(channel_info_offset + 0x00,sf);
            uint32_t channel_dsp_offset  = info_offset + read_u32be(channel_info_offset + 0x04,sf);

            /* always been 0... */
            if (read_u32be(channel_info_offset + 0x8,sf) != 0) goto fail;

            vgmstream->ch[i].streamfile = reopen_streamfile(sf, 0);
            if (!vgmstream->ch[i].streamfile) goto fail;
            vgmstream->ch[i].channel_start_offset = vgmstream->ch[i].offset = channel_data_offset;

            for (j = 0; j < 16; j++) {
                vgmstream->ch[i].adpcm_coef[j] = read_s16be(channel_dsp_offset + j*0x02,sf);
            }
        }
    }

    //if (!vgmstream_open_stream(vgmstream, sf, start_offset))
    //    goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
