#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"


/* BKHD - Wwise soundbank container */
VGMSTREAM* init_vgmstream_bkhd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, base_offset = 0;
    uint32_t subfile_id, version;
    int big_endian, is_dummy = 0, is_wmid = 0;
    uint32_t (*read_u32)(off_t,STREAMFILE*);
    float (*read_f32)(off_t,STREAMFILE*);
    int total_subsongs, target_subsong = sf->stream_index;
    int prefetch = 0;


    /* checks */
    if (!check_extensions(sf,"bnk"))
        goto fail;

    if (is_id32be(0x00, sf, "AKBK")) /* [Shadowrun (X360)] */
        base_offset = 0x0c;
    if (!is_id32be(base_offset + 0x00, sf, "BKHD"))
        goto fail;
    big_endian = guess_endianness32bit(base_offset + 0x04, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;
    read_f32 = big_endian ? read_f32be : read_f32le;


    /* Wwise banks have event/track/sequence/etc info in the HIRC chunk, as well
     * as other chunks, and may have a DATA/DIDX index to memory .wem in DATA.
     * We support the internal .wem mainly for quick tests, as the HIRC is
     * complex and better handled with TXTP (some info from Nicknine's script).
     * Use this to explore HIRC and covert to .txtp: https://github.com/bnnm/wwiser */

    version = read_u32(base_offset + 0x08, sf); /* rarely version can be encrypted, but ok as u32 [Tamarin (PC)] */
    if (version == 0 || version == 1) { /* early games */
        version = read_u32(base_offset + 0x10, sf);
    }

    /* first chunk also follows standard chunk sizes unlike RIFF */
    if (version <= 26) {
        off_t data_offset_off;
        uint32_t data_offset, data_start, offset;
        if (!find_chunk(sf, 0x44415441, base_offset, 0, &data_offset_off, NULL, big_endian, 0)) /* "DATA" */
            goto fail;
        data_offset = data_offset_off;

        /* index:
         * 00: entries
         * 04: null
         * 08: entries size
         * 0c: padding size after entries
         * 10: data size
         * 14: size? or null
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
        enum {
            CHUNK_DIDX = 0x44494458, /* "DIDX" */
            CHUNK_DATA = 0x44415441, /* "DATA" */
        };
        uint32_t didx_offset = 0, data_offset = 0, didx_size = 0, offset;
        chunk_t rc = {0};

        rc.be_size = big_endian;
        rc.current = 0x00;
        while (next_chunk(&rc, sf)) {
            switch(rc.type) {

                case CHUNK_DIDX:
                    didx_offset = rc.offset;
                    didx_size = rc.size;
                    break;

                case CHUNK_DATA:
                    data_offset = rc.offset;
                    break;

                default:
                    break;
            }
        }
        if (!didx_offset || !data_offset)
            goto fail;

        total_subsongs = didx_size / 0x0c;
        if (total_subsongs < 1) {
            vgm_logi("BKHD: bank has no subsongs (ignore)\n");
            goto fail;
        }
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong > total_subsongs) goto fail;

        offset = didx_offset + (target_subsong - 1) * 0x0c;
        subfile_id      = read_u32(offset + 0x00, sf);
        subfile_offset  = read_u32(offset + 0x04, sf) + data_offset;
        subfile_size    = read_u32(offset + 0x08, sf);
    }

    //;VGM_LOG("BKHD: %x, %x\n", subfile_offset, subfile_size);

    /* detect format */
    if (subfile_offset <= 0 || subfile_size <= 0) {
        is_dummy = 1;
        /* rarely some indexes don't have data (early bnk)
         * for now leave a dummy song for easier .bnk index-to-subsong mapping */
        vgmstream = init_vgmstream_silence(0, 0, 0);
        if (!vgmstream) goto fail;
    }
    else {
        /* could pass .wem extension but few files need memory .wem detection */
        temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
        if (!temp_sf) goto fail;

        /* read using temp_sf in case of >2GB .bnk */
        if (is_id32be(0x00, temp_sf, "RIFF") || is_id32be(0x00, temp_sf, "RIFX")) {
            vgmstream = init_vgmstream_wwise_bnk(temp_sf, &prefetch);
            if (!vgmstream) goto fail;
        }
        else if (read_f32(subfile_offset + 0x02, temp_sf) >= 30.0 &&
                 read_f32(subfile_offset + 0x02, temp_sf) <= 250.0) {
            is_wmid = 1;
            /* ignore Wwise's custom .wmid (similar to a regular midi but with simplified
             *  chunks and custom fields: 0x00=MThd's division, 0x02: bpm (new), etc) */
            vgmstream = init_vgmstream_silence(0, 0, 0);
            if (!vgmstream) goto fail;
        }
        else {
            /* may fail if not an actual wfx */
            vgmstream = init_vgmstream_bkhd_fx(temp_sf);
            if (!vgmstream) goto fail;
        }
    }

    vgmstream->num_streams = total_subsongs;

    {
        const char* info = NULL;
        if (is_dummy)
            info = "dummy";
        else if (is_wmid)
            info = "wmid";

        /* old Wwise shows index or (more often) -1, unify to index*/
        if (subfile_id == 0xFFFFFFFF)
            subfile_id = target_subsong - 1;

        if (info)
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%u/%s", subfile_id, info);
        else
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%u", subfile_id);
        if (prefetch)
            concatn(STREAM_NAME_SIZE, vgmstream->stream_name, " [pre]");
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


/* BKHD mini format, for FX plugins [Borderlands 2 (X360), Warhammer 40000 (PC)] */
VGMSTREAM* init_vgmstream_bkhd_fx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int big_endian, loop_flag, channels, sample_rate, entries;
    uint32_t (*read_u32)(off_t,STREAMFILE*);


    /* checks */
    /* .wem: used when (rarely) external */
    if (!check_extensions(sf,"wem,bnk"))
        goto fail;
    big_endian = guess_endianness32bit(0x00, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;

    /* Not an actual stream but typically convolution reverb models and other FX plugin helpers.
     * Useless but to avoid "subsong not playing" complaints. */

    /* Wwise Convolution Reverb */
    if ((read_u32(0x00, sf) == 0x00000400 ||    /* common */
         read_u32(0x00, sf) == 0x00020400 ) &&  /* Elden Ring */
        read_u32(0x04, sf) == 0x0800) {
        sample_rate = read_u32(0x08, sf);
        channels    = read_u32(0x0c, sf) & 0xFF; /* 0x31 at 0x0d in PC, field is 32b vs X360 */

        /* 0x10: some id or small size? (related to entries?) */
        /* 0x14/18: some float? */
        entries     = read_u32(0x1c, sf);
        /* 0x20 data size / 0x10 */
        /* 0x24 usually 4, sometimes higher values? */
        /* 0x30: unknown table of 16b that goes up and down, or is fixed */

        start_offset = 0x30 + align_size_to_block(entries * 0x02, 0x10);
        data_size = get_streamfile_size(sf) - start_offset;
    }
    else if (read_u32be(0x04, sf) == 0x00004844 && /* floats actually? */
             read_u32be(0x08, sf) == 0x0000FA45 &&
             read_u32be(0x1c, sf) == 0x80000000) {
        /* seen in Crucible banks */
        sample_rate = 48000; /* meh */
        channels    = 1;

        start_offset = 0;
        data_size = get_streamfile_size(sf);
        big_endian = 0;
    }
    else {
        goto fail;
    }

    loop_flag = 0;


    /* data seems divided in chunks of 0x2000 */

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

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
