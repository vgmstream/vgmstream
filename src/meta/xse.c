#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* SDRH - banks for newer feelplus-related games [Mindjack (PS3/X360)] */
VGMSTREAM* init_vgmstream_xse_new(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size, stream_size;
    int loop_flag = 0, channels, codec, sample_rate, seek_count;
    int32_t num_samples, loop_start, loop_end;
    off_t offset;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "xse"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x48524453) /* "HRDS" */
        goto fail;

    /* similar to older version but BE and a bit less complex */
    /* 0x04: version/config?
     * 0x08: data size
     * 0x30: file name in some strange encoding/compression?
     * others: ? (change in old/new)
    */

    /* parse section */
    {
        int i;
        int tables = read_u16be(0x1C,sf);
        off_t base_size, stream_offset;
        int entries;

        offset = 0;
        /* read sections (FE=cues?, WV=mini-headers?, XW=waves) */
        for (i = 0; i < tables; i++) {
            uint16_t id = read_u16be(0x40 + 0x10 * i + 0x00,sf);
            /* 0x02: offset in 0x40s */
            /* 0x04: section size */
            /* 0x08: always 1 */
            /* 0x0c: null */
            if (id == 0x5857)  { /* "XW" */
                offset += read_u16be(0x40 + 0x10 * i + 0x02,sf) * 0x40;
                break;
            }
        }

        /* section header (other sections have a similar header) */
        /* 0x00: section size */
        base_size    = read_u16be(offset + 0x04,sf);
        entries      = read_u16be(offset + 0x06,sf);
        /* 0x08: null */
        start_offset = read_u32be(offset + 0x0c,sf) + offset; /* size including padding up to start */

        offset += base_size;

        total_subsongs = entries;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        /* find stream header (entries can be variable-sized) */
        for (i = 0; i < entries; i++) {
            size_t seek_size = read_u16be(offset + 0x0a,sf) * 0x04;
            size_t entry_size = align_size_to_block(0x30 + seek_size, 0x10);

            if (i + 1 == target_subsong)
                break;
            offset += entry_size;
        }

        /* parse target header (similar to xwav) */
        stream_size = read_u32be(offset + 0x00,sf);
        /* 0x04: codec? (16=PS3, 03=X360) */
        codec       =  read_u8(offset + 0x06,sf); /* assumed */
        loop_flag   =  read_u8(offset + 0x07,sf); /* assumed */
        /* 0x08: bps? */
        channels    =  read_u8(offset + 0x09,sf);
        seek_count  = read_u16be(offset + 0x0a,sf);
        num_samples = read_u32be(offset + 0x0c,sf);
        sample_rate = read_u32be(offset + 0x10,sf);
        loop_start  = read_u32be(offset + 0x14,sf);
        loop_end    = read_u32be(offset + 0x18,sf);
        /* 0x1c: ? */
        stream_offset = read_u32be(offset + 0x20,sf); /* within data */
        /* 0x24: ? */
        /* 0x26 seek entries */
        /* 0x28: ? */
        /* 0x2c: null? */

        start_offset += stream_offset;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDRH;
    vgmstream->num_samples = num_samples;
    vgmstream->sample_rate = sample_rate;

    vgmstream->stream_size = stream_size;
    vgmstream->num_streams = total_subsongs;

    switch(codec) {
        case 2: /* Mindjack (PS3) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            break;

#ifdef VGM_USE_FFMPEG
        case 1: { /* Mindjack (X360) */
            uint8_t buf[0x100];
            int32_t bytes, block_size, block_count;

            data_size = get_streamfile_size(sf) - start_offset;
            block_size = 0x10000; /* XWAV new default */
            block_count = seek_count;

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            //todo fix loops/samples vs ATRAC3
            /* may be only applying end_skip to num_samples? */
            xma_fix_raw_samples(vgmstream, sf, start_offset,data_size, 0, 0,0);
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* SDRH - banks for older feelplus-related games [Lost Odyssey (X360)] */
VGMSTREAM* init_vgmstream_xse_old(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size, stream_size;
    int loop_flag = 0, channels, codec, sample_rate, seek_count;
    int32_t num_samples, loop_start, loop_end;
    off_t offset;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    /* .xse: assumed */
    if (!check_extensions(sf, "xse"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x53445248) /* "SDRH" */
        goto fail;

    /* similar to older version but LE and a bit more complex */
    /* 0x04: version/config?
     * 0x08: data size
     * 0x30: file name in some strange encoding/compression?
     * others: ? (change in old/new)
    */

    /* parse section */
    {
        int i;
        int tables = read_u8(0x15,sf);
        off_t base_size, stream_offset;
        int entries;

        offset = 0x40;
        /* read sections (FE=cues?, WV=mini-headers?, FT=?, FQ=?, XW=waves) */
        for (i = 0; i < tables; i++) {
            uint16_t id = read_u16be(0x40 + 0x08 * i + 0x00,sf);
            /* 0x02: null? */
            /* 0x04: offset from table start */
            if (id == 0x5857) { /* "XW" */
                offset += read_u32le(0x40 + 0x08 * i + 0x04,sf);
                break;
            }
        }

        /* section header (other sections have a similar header) */
        /* 0x00: section size */
        base_size    = read_u16le(offset + 0x04,sf);
        /* 0x06: ? */
        entries      = read_u16le(offset + 0x08,sf);
        start_offset = read_u32le(offset + 0x0c,sf) + offset; /* size including padding up to start */

        offset += base_size;

        total_subsongs = entries;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        /* find stream header */
        stream_offset = 0;
        for (i = 0; i < entries; i++) {
            size_t data_size = read_u32le(offset + 0x00,sf) - 0x30;
            size_t seek_size = 0; //read_u16be(offset + 0x0a,sf) * 0x04; /* not seen */
            size_t entry_size = align_size_to_block(0x30 + seek_size, 0x10);

            if (i + 1 == target_subsong)
                break;
            offset += entry_size;
            stream_offset += data_size; /* no offset? */
        }

        /* parse target header (similar to xwav) */
        stream_size = read_u32le(offset + 0x00,sf) - 0x30; /* adds entry size */
        /* 0x04: codec? (16=PS3, 03=X360) */
        codec       =    read_u8(offset + 0x06,sf); /* assumed */
        /* 0x07: flag? */
        /* 0x08: bps? */
        /* 0x09: codec? */
        /* 0x0a: null */
        num_samples = read_u32le(offset + 0x0c,sf);
        seek_count  = read_u16le(offset + 0x10,sf);
        sample_rate = read_u32le(offset + 0x14,sf);
        loop_start  = 0; //read_u32le(offset + 0x18,sf); /* ? */
        loop_end    = 0; //read_u32le(offset + 0x1c,sf); /* ? */
        /* 0x20: null */
        /* 0x24: ? */
        /* 0x26: channel layout */
        channels    =    read_u8(offset + 0x27,sf);
        /* 0x28: ? */
        /* 0x2c: null? */

        loop_flag = loop_end > 0;

        start_offset += stream_offset;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDRH;
    vgmstream->num_samples = num_samples;
    vgmstream->sample_rate = sample_rate;

    vgmstream->stream_size = stream_size;
    vgmstream->num_streams = total_subsongs;

    switch(codec) {

#ifdef VGM_USE_FFMPEG
        case 4: { /* Lost Odyssey (X360) */
            uint8_t buf[0x100];
            int32_t bytes, block_size, block_count;

            data_size = get_streamfile_size(sf) - start_offset;
            block_size = 0x8000; /* XWAV old default */
            block_count = seek_count;

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, 0, 0, 1);
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
