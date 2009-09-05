#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* BNS - Wii "Banner Sound" disc jingle */
VGMSTREAM * init_vgmstream_wii_bns(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t BNS_offset;
    uint32_t info_offset=0,data_offset=0;
    uint32_t channel_info_offset_list_offset;
	int channel_count;
    int loop_flag;
    uint16_t sample_rate;
    uint32_t sample_count, loop_start;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bns",filename_extension(filename))) goto fail;

    // check header
    BNS_offset = 0;
    if (read_32bitBE(BNS_offset,streamFile) == 0x494D4435) // IMD5
    {
        // Skip IMD5 header if present
        BNS_offset = 0x20;
    }

    if (read_32bitBE(BNS_offset+0x00,streamFile) != 0x424E5320) goto fail; // "BNS "
    if ((uint32_t)read_32bitBE(BNS_offset+0x04,streamFile) != 0xFEFF0100u) goto fail;

    // find chunks, verify
    {
        // file size as claimed by header
        uint32_t header_file_size = read_32bitBE(BNS_offset+0x08,streamFile);

        uint32_t header_size = read_16bitBE(BNS_offset+0xc,streamFile);
        uint16_t chunk_count = read_16bitBE(BNS_offset+0xe,streamFile);

        int i;

        // assume BNS is the last thing in the file
        if (header_file_size + BNS_offset != get_streamfile_size(streamFile))
            goto fail;

        for (i = 0; i < chunk_count; i++) {
            uint32_t chunk_info_offset = BNS_offset+0x10+i*8;
            uint32_t chunk_offset, chunk_size;

            // ensure chunk info is within header
            if (chunk_info_offset+8 > BNS_offset+header_size) goto fail;

            chunk_offset = BNS_offset + (uint32_t)read_32bitBE(chunk_info_offset,streamFile);
            chunk_size = (uint32_t)read_32bitBE(chunk_info_offset+4,streamFile);

            // ensure chunk is within file
            if (chunk_offset < BNS_offset+header_size ||
                chunk_offset+chunk_size > BNS_offset+header_file_size) goto fail;

            // ensure chunk size in header matches that listed in chunk
            // Note: disabled for now, as the Homebrew Channel BNS has a DATA
            // chunk that doesn't include the header size
            //if ((uint32_t)read_32bitBE(chunk_offset+4,streamFile) != chunk_size) goto fail;

            // handle each chunk type
            switch (read_32bitBE(chunk_offset,streamFile)) {
                case 0x494E464F:    // INFO
                    info_offset = chunk_offset+8;
                    break;
                case 0x44415441:    // DATA
                    data_offset = chunk_offset+8;
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
        if (read_8bit(info_offset+0x00,streamFile) != 0) goto fail;

        loop_flag = read_8bit(info_offset+0x01,streamFile);
        channel_count = read_8bit(info_offset+0x02,streamFile);

        /* only seen zero, padding? */
        if (read_8bit(info_offset+0x03,streamFile) != 0) goto fail;

        sample_rate = (uint16_t)read_16bitBE(info_offset+0x04,streamFile);

        /* only seen this zero, padding? */
        if (read_16bitBE(info_offset+0x06,streamFile) != 0) goto fail;

        loop_start = read_32bitBE(info_offset+0x08,streamFile);

        sample_count = read_32bitBE(info_offset+0x0c,streamFile);

        channel_info_offset_list_offset = info_offset + (uint32_t)read_32bitBE(info_offset+0x10,streamFile);
    }

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = sample_count;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_WII_BNS;

    if (loop_flag)
    {
        vgmstream->loop_start_sample = loop_start;
        vgmstream->loop_end_sample = sample_count;
    }

    /* open the file for reading */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            uint32_t channel_info_offset = info_offset + read_32bitBE(channel_info_offset_list_offset+4*i,streamFile);
            uint32_t channel_data_offset = data_offset + (uint32_t)read_32bitBE(channel_info_offset+0,streamFile);
            uint32_t channel_dsp_offset = info_offset + (uint32_t)read_32bitBE(channel_info_offset+4,streamFile);
            int j;

            /* always been 0... */
            if (read_32bitBE(channel_info_offset+8,streamFile) != 0) goto fail;

            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=channel_data_offset;

            for (j=0;j<16;j++)
                vgmstream->ch[i].adpcm_coef[j] =
                    read_16bitBE(channel_dsp_offset+j*2,streamFile);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
