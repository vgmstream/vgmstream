#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* SDRH - banks for newer feelplus-related ("FeelEngine") games [Mindjack (PS3/X360), Moon Diver (PS3/X360)] */
VGMSTREAM* init_vgmstream_sdrh_new(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, stream_size;
    int loop_flag = 0, channels, codec, sample_rate, seek_count;
    int32_t num_samples, loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32le(0x00,sf, "SDRH")) /* LE */
        goto fail;
    if (!check_extensions(sf, "xse"))
        goto fail;

    /* similar to older version but BE and a bit less complex */
    /* 0x04: version/config?
     * 0x08: data size
     * 0x30: file name in a custom 40-char (RADIX style) encoding
     * others: ?
     */

    /* parse section */
    {
        uint32_t offset, base_size, stream_offset, data_size = 0, entry_size = 0;
        int i;
        int tables = read_u16be(0x1C,sf);
        int entries;

        offset = 0;
        /* read sections: FE=cues? (same in platforms), WV=header position + sample rate?, FT=? (same in platforms), XW=waves */
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
        /* 0x00: section size (including data) */
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
            entry_size = read_u16be(offset + 0x04,sf) * 0x10;

            if (i + 1 == target_subsong)
                break;
            offset += entry_size;
        }

        /* parse target header (similar to xwav) */
        data_size = read_u32be(offset + 0x00,sf) - entry_size;
        /* 0x00: data/entry size */
        /* 0x04: entry size */
        codec       =    read_u8(offset + 0x06,sf); /* assumed */
        loop_flag   =    read_u8(offset + 0x07,sf); /* assumed */
        /* 0x08: bps? flags? */
        channels    =    read_u8(offset + 0x09,sf);
        seek_count  = read_u16be(offset + 0x0a,sf);
        num_samples = read_u32be(offset + 0x0c,sf);

        sample_rate = read_u32be(offset + 0x10,sf);
        loop_start  = read_u32be(offset + 0x14,sf);
        loop_end    = read_u32be(offset + 0x18,sf);
        /* 0x1c: flags? */

        stream_offset = read_u32be(offset + 0x20,sf);

        /* Mindjack uses full offsets here (aligned to 0x800), while Moon Diver points to a sub-offset
         * (offsets also aren't ordered) */
        if ((stream_offset & 0x000007FF) != 0) {
            stream_offset = read_u16be(offset + stream_offset, sf) * 0x800;
            //TODO some files that loop have wrong size/num_samples? (ex.MJ system.xse #23 #138)
        } 

        /* other values change per game (seek table, etc) */

        start_offset += stream_offset;
        stream_size = data_size;
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
        case 2: /* Mindjack (PS3), Moon Diver (PS3) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            /* seen in a few Mindjack files */
            if (loop_end > num_samples && loop_end - 8 <= num_samples)
                loop_end = num_samples;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            break;

#ifdef VGM_USE_MPEG
        case 5: {    /* No More Heroes (PS3) (rare, BAITO_GOMIHORI.XSE) */
            mpeg_custom_config cfg = {0};
            cfg.data_size = stream_size;

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case 1: { /* Mindjack (X360), Moon Diver (X360) */
            int block_size = 0x10000; /* XWAV new default */
            int block_count = seek_count;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, start_offset, stream_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, block_count);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            //TODO fix loops/samples vs ATRAC3
            /* may be only applying end_skip to num_samples? */
            xma_fix_raw_samples(vgmstream, sf, start_offset, stream_size, 0, 0,0);
            break;
        }

        case 6:   /* No More Heroes (PS3) */
        case 7:   /* No More Heroes (PS3), Mindjack (PS3) */
        case 8: { /* No More Heroes (PS3) */
            int block_align, encoder_delay;

            /* fixed for all rates? doesn't happen with other codecs, some files are 48000 already */
            vgmstream->sample_rate = 48000;

            block_align = (codec == 8 ? 0xC0 : codec == 0x07 ? 0x98 : 0x60) * vgmstream->channels;
            encoder_delay = 1024 + 69*2; /* observed default, but seems files run out of space */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset, stream_size, vgmstream->num_samples, vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* set offset samples (offset 0 jumps to sample 0 > pre-applied delay, and offset end loops after sample end > adjusted delay) */
            vgmstream->loop_start_sample = atrac3_bytes_to_samples(loop_start, block_align); //- encoder_delay
            vgmstream->loop_end_sample   = atrac3_bytes_to_samples(loop_end, block_align) - encoder_delay;
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


/* SDRH - banks for older feelplus-related games [Lost Odyssey (X360), Lost Odyssey Demo (X360)] */
VGMSTREAM* init_vgmstream_sdrh_old(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, stream_size;
    int loop_flag = 0, channels = 0, codec, sample_rate, seek_count;
    int32_t num_samples, loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "SDRH"))
        goto fail;

    /* .xse: actual extension (LO demo) */
    if (!check_extensions(sf, "xse"))
        goto fail;

    /* similar to older version but LE and a bit more complex */
    /* 0x04: version/config?
     * 0x08: data size
     * 0x30: file name in a custom 40-char (RADIX style) encoding
     * others: ? (change in old/new)
    */

    /* parse section */
    {
        uint32_t offset, base_size, stream_offset, data_size = 0, entry_size = 0;
        int i;
        int tables = read_u8(0x15,sf);
        int entries;

        offset = 0x40;
        /* read sections (FE=cues?, WV=mini-headers + XW offset, FT=?, FQ=?, XW=waves) */
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
        /* 0x00: section size (including data) */
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
            data_size = read_u32le(offset + 0x00,sf);
            entry_size = read_u16le(offset + 0x04,sf) * 0x10;
            data_size -= entry_size;

            if (i + 1 == target_subsong)
                break;
            offset += entry_size;
            stream_offset += data_size; /* no offset */
        }

        /* parse target header (similar to xwav) */
        /* 0x00: data size + entry size */
        /* 0x04: entry size */
        codec       =    read_u8(offset + 0x06,sf); /* assumed */
        /* 0x07: flag? */
        /* 0x08: bps? */
        /* 0x09: codec? */
        /* 0x0a: null (seek size?) */
        num_samples = read_u32le(offset + 0x0c,sf); /* XMA1: loop start/end info */

        seek_count  = read_u16le(offset + 0x10,sf); /* XMA1: loop start/end bytes? */
        sample_rate = read_u16le(offset + 0x14,sf);
        if (entry_size == 0x20) {
            channels    =    read_u8(offset + 0x17,sf);
        }
        loop_start  = 0; //read_u32le(offset + 0x18,sf); /* ? */
        loop_end    = 0; //read_u32le(offset + 0x1c,sf); /* ? */

        if (entry_size >= 0x30) {
            /* 0x20: null */
            /* 0x24: ? */
            /* 0x26: channel layout */
            channels    =    read_u8(offset + 0x27,sf);
            /* 0x28: ? */
            /* 0x2c: null? */
        }

        loop_flag = loop_end > 0;

        start_offset += stream_offset;
        stream_size = data_size;
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
        case 1: { /* Lost Odyssey Demo (X360) */
            vgmstream->codec_data = init_ffmpeg_xma1_raw(sf, start_offset, stream_size, vgmstream->channels, vgmstream->sample_rate, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            {
                ms_sample_data msd = {0};

                msd.xma_version = 1;
                msd.channels    = channels;
                msd.data_offset = start_offset;
                msd.data_size   = stream_size;
                msd.loop_flag   = loop_flag;
                msd.loop_start_b= 0; //loop_start_b;
                msd.loop_end_b  = 0; //loop_end_b;
                msd.loop_start_subframe = 0; //loop_subframe & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
                msd.loop_end_subframe   = 0; //loop_subframe >> 4; /* upper 4b: subframe where the loop ends, 0..3 */
                msd.chunk_offset = 0;

                xma_get_samples(&msd, sf);

                vgmstream->num_samples = msd.num_samples;
                //loop_start_sample = msd.loop_start_sample;
                //loop_end_sample = msd.loop_end_sample;
            }


            xma_fix_raw_samples(vgmstream, sf, start_offset, stream_size, 0, 0, 1);
            break;
        }
        case 4: { /* Lost Odyssey (X360) */
            int block_size = 0x8000; /* XWAV old default */
            int block_count = seek_count;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, start_offset, stream_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, block_count);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            xma_fix_raw_samples(vgmstream, sf, start_offset, stream_size, 0, 0, 1);
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
