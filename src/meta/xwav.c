#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* XWAV - streams for newer feelplus-related games [No More Heroes: Heroes Paradise (PS3/X360), Moon Diver (PS3/X360)] */
VGMSTREAM* init_vgmstream_xwav_new(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag = 0, channels, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end;


    /* checks */
    /* .xwv: actual extension [Moon Diver (PS3/X360)]
     * .vawx: header id */
    if (!check_extensions(sf, "xwv,vawx"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x56415758) /* "VAWX" */
        goto fail;

    /* similar to older version but BE and a bit less complex */
    /* 0x04: data size
     * 0x08: version (always 3)
     * 0x0a: sub-version (0 in NMH/NNN2, 5 in MD)
     * 0x0c: ? (0080 + some value)
     * 0x10: ? (00402000)
     * 0x14: ? (3380)
     * 0x16: file number
     * 0x18: null
     * 0x1c: null
     * 0x20: file name in some strange encoding/compression?
    */
    start_offset = 0x800;

    /* parse header */
    {
        /* 0x00: stream size */
        /* 0x04: ? */
        codec       =    read_u8(0x30 + 0x06,sf);
        loop_flag   =    read_u8(0x30 + 0x07,sf);
        /* 0x08: ? */
        channels    =    read_u8(0x30 + 0x09,sf);
        /* 0x0a: seek entries */
        num_samples = read_u32be(0x30 + 0x0c,sf);
        sample_rate = read_u32be(0x30 + 0x10,sf);
        loop_start  = read_u32be(0x30 + 0x14,sf);
        loop_end    = read_u32be(0x30 + 0x18,sf);
        /* rest: ? (also see xse) */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XWAV;
    vgmstream->num_samples = num_samples;
    vgmstream->sample_rate = sample_rate;

    switch(codec) {
        case 2: /* No Nore Heroes (PS3) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = channels == 6 ? layout_blocked_xwav : layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            break;

#ifdef VGM_USE_FFMPEG
        case 1: { /* No Nore Heroes (X360), Moon Diver (X360), Ninety-Nine Nights 2 (X360) */
            uint8_t buf[0x100];
            int32_t bytes, block_size, block_count;

            data_size = get_streamfile_size(sf) - start_offset;
            block_size = 0x10000; /* XWAV new default */
            block_count = read_u16be(0x30 + 0x0A, sf); /* also at 0x56 */

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

        case 7: { /* Moon Diver (PS3) */
            int block_align, encoder_delay;

            data_size = read_u32be(0x54,sf);
            block_align = 0x98 * vgmstream->channels;
            encoder_delay = 1024 + 69*2; /* observed default, matches XMA (needed as many files start with garbage) */
            vgmstream->num_samples = atrac3_bytes_to_samples(data_size, block_align) - encoder_delay; /* original samples break looping in some files otherwise */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
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

/* XWAV - streams for older feelplus-related games [Bullet Witch (X360), Lost Odyssey (X360)] */
VGMSTREAM* init_vgmstream_xwav_old(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag = 0, channels, codec, tracks, sample_rate;
    int32_t num_samples, loop_start, loop_end;


    /* checks */
    /* .xwv: actual extension [Bullet Witch (X360)] */
    if (!check_extensions(sf, "xwv"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x58574156) /* "XWAV" */
        goto fail;

    /* similar to newer version but LE and a bit more complex */
    /* 0x04: data size
     * 0x08: version (always 2)
     * 0x0a: sub-version? (0x100/200/300 in LO, 0x200 in BW)
     * 0x0c: ?
     * 0x10: start offset (in 0x10s)
     * 0x12: ? (low number)
     * 0x20: stream size
     * 0x24: ?
     * 0x26: codec?
     * 0x27: tracks
     * rest varies depending on codec
    */
    start_offset = read_u16le(0x10,sf) * 0x10;

    codec = read_u8(0x26,sf);
    tracks = read_u8(0x27,sf);

    switch(codec) {
        case 2: /* PSX */
            /* 0x2c: null? */
            num_samples = read_u32le(0x30,sf);
            sample_rate = read_u16le(0x34,sf);
            channels    =    read_u8(0x37,sf);
            loop_start  = read_u32le(0x38,sf);
            loop_end    = read_u32le(0x3c,sf);
            if (tracks > 1)
                goto fail;
            break;

        case 4: /* XMA */
            num_samples = read_u32le(0x2c,sf);
            /* 0x30: xma blocks of 0x8000 */
            sample_rate = read_u16le(0x34,sf);
            /* 0x38: ? (0x10/20) */
            /* 0x3c: null */
            loop_start  = read_u32le(0x48,sf); /* per stream, but all should match */
            loop_end    = read_u32le(0x4C,sf);

            /* listed as XMA streams like XMA1, but XMA2 shouldn't need this (uses proper Nch XMA2) */
            {
                channels = 0;
                for (int i = 0; i < tracks; i++) {
                    /* 0x00: null */
                    /* 0x04: null */
                    /* 0x06: channel layout null */
                    channels += read_u8(0x40 + 0x10 * i + 0x07,sf);
                    /* 0x08: loop start */
                    /* 0x0c: loop end */
                }
            }

            /* next is a seek table, padded to 0x10 */
            break;

        default:
            goto fail;
    }

    loop_flag = loop_end > 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XWAV;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    switch(codec) {
        case 2: /* Bullet Witch (X360) (seems unused as there are .xwb) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);
            break;

#ifdef VGM_USE_FFMPEG
        case 4: { /* Lost Odyssey (X360) */
            uint8_t buf[0x100];
            int32_t bytes, block_size, block_count;

            data_size = get_streamfile_size(sf) - start_offset;
            block_size = 0x8000; /* XWAV old default */
            block_count = read_u16be(0x30, sf);

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
