#include "meta.h"
#include "../coding/coding.h"


/* BKHD - Wwise soundbank container */
VGMSTREAM* init_vgmstream_bkhd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset, base_offset = 0;
    size_t subfile_size;
    uint32_t subfile_id, header_id;
    int big_endian, version, is_dummy = 0;
    uint32_t (*read_u32)(off_t,STREAMFILE*);
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf,"bnk"))
        goto fail;

    if (read_u32be(0x00, sf) == 0x414B424B) /* "AKBK" [Shadowrun (X360)] */
        base_offset = 0x0c;
    if (read_u32be(base_offset + 0x00, sf) != 0x424B4844) /* "BKHD" */
        goto fail;
    big_endian = guess_endianness32bit(base_offset + 0x04, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;


    /* Wwise banks have event/track/sequence/etc info in the HIRC chunk, as well
     * as other chunks, and may have a DATA/DIDX index to memory .wem in DATA.
     * We support the internal .wem mainly for quick tests, as the HIRC is
     * complex and better handled with TXTP (some info from Nicknine's script).
     * unlike RIFF, first chunk follows chunk rules */

    version = read_u32(base_offset + 0x08, sf);
    if (version == 0 || version == 1) { /* early games */
        version = read_u32(base_offset + 0x10, sf);
    }

    if (version <= 26) {
        off_t data_offset, data_start, offset;
        if (!find_chunk(sf, 0x44415441, base_offset, 0, &data_offset, NULL, big_endian, 0)) /* "DATA" */
            goto fail;

        /* index:
         * 00: entries
         * 04: null
         * 08: entries size
         * 0c: padding size after entries
         * 10: data size
         * 14: size?
         * 18: data start
         * 1c: data size
         * per entry:
         *  00: always -1
         *  04: always 0
         *  08: index number or -1
         *  0c: 5 or -1?
         *  0c: 5 or -1?
         *  0c: 5 or -1?
         *  10: stream offset (from data start) or -1 if none
         *  14: stream size or 0 if none
         */

        total_subsongs = read_u32(data_offset + 0x00, sf);
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        data_start = read_u32(data_offset + 0x18, sf);
        offset = data_offset + 0x20 + (target_subsong - 1) * 0x18;

        subfile_id      = read_u32(offset + 0x08, sf);
        subfile_offset  = read_u32(offset + 0x10, sf) + data_offset + 0x20 + data_start;
        subfile_size    = read_u32(offset + 0x14, sf);
    }
    else {
        off_t didx_offset, data_offset, offset;
        size_t didx_size;
        if (!find_chunk(sf, 0x44494458, 0x00,0, &didx_offset, &didx_size, big_endian, 0)) /* "DIDX" */
            goto fail;
        if (!find_chunk(sf, 0x44415441, 0x00,0, &data_offset, NULL, big_endian, 0)) /* "DATA" */
            goto fail;

        total_subsongs = didx_size / 0x0c;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        offset = didx_offset + (target_subsong - 1) * 0x0c;
        subfile_id      = read_u32(offset + 0x00, sf);
        subfile_offset  = read_u32(offset + 0x04, sf) + data_offset;
        subfile_size    = read_u32(offset + 0x08, sf);
    }

    /* some indexes don't have that, but for now leave a dummy song for easier HIRC mapping */
    if (subfile_offset <= 0 || subfile_size <= 0) {
        //;VGM_LOG("BKHD: dummy entry");
        temp_sf = setup_subfile_streamfile(sf, 0x00, 0x10, "raw");
        if (!temp_sf) goto fail;

        //todo make some better silent entry
        vgmstream = init_vgmstream_raw_pcm(temp_sf);
        if (!vgmstream) goto fail;
        
        is_dummy = 1;
    }
    else {
        //;VGM_LOG("BKHD: %lx, %x\n", subfile_offset, subfile_size);
        /* could pass .wem but few files need memory .wem detection */
        temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
        if (!temp_sf) goto fail;

        header_id = read_u32be(0x00, temp_sf);
        if (header_id == 0x52494646 || header_id == 0x52494658) { /* "RIFF" / "RIFX" */
            vgmstream = init_vgmstream_wwise(temp_sf);
            if (!vgmstream) goto fail;
        }
        else {
            vgmstream = init_vgmstream_bkhd_fx(temp_sf);
            if (!vgmstream) goto fail;
        }
    }


    vgmstream->num_streams = total_subsongs;
    
    if (is_dummy)
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", "dummy");
    else if (subfile_id != 0xFFFFFFFF)
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%u", subfile_id);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


/* BKHD mini format, probably from a FX generator plugin [Borderlands 2 (X360)] */
VGMSTREAM* init_vgmstream_bkhd_fx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int big_endian, loop_flag, channels, sample_rate, entries;
    uint32_t (*read_u32)(off_t,STREAMFILE*);


    /* checks */
    if (!check_extensions(sf,"wem,bnk")) /* assumed */
        goto fail;
    big_endian = guess_endianness32bit(0x00, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;

    if (read_u32(0x00, sf) != 0x0400) /* codec? */
        goto fail;
    if (read_u32(0x04, sf) != 0x0800) /* codec? */
        goto fail;
    sample_rate = read_u32(0x08, sf);
    channels    = read_u32(0x0c, sf);
    /* 0x10: some id or small size? */
    /* 0x14/18: some float? */
    entries     = read_u32(0x1c, sf);
    /* 0x20 data size / 0x10 */
    /* 0x24 usually 4, sometimes higher values? */
    /* 0x30: unknown table of 16b that goes up and down */

    start_offset = 0x30 + align_size_to_block(entries * 0x02, 0x10);
    data_size = get_streamfile_size(sf) - start_offset;
    loop_flag = 0;

    /* output sounds a bit funny, maybe not an actual stream but parts using the table */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WWISE_FX;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_PCMFLOAT;
    vgmstream->layout_type = layout_interleave;
    vgmstream->codec_endian = big_endian;
    vgmstream->interleave_block_size = 0x4;

    vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 32);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
