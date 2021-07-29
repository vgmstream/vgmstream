#include "meta.h"
#include "../coding/coding.h"

/* WBAT - Firebrand Games header [Need for Speed: The Run (3DS), Fast & Furious: Showdown (3DS)] */
VGMSTREAM * init_vgmstream_wavebatch(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, name_offset, offset, stream_offset;
    size_t names_size, stream_size;
    int loop_flag, channel_count, sample_rate, num_samples;
    int big_endian, version, codec;
    int total_subsongs, target_subsong = sf->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(sf, "wavebatch"))
        goto fail;
    if (!is_id32be(0x00,sf, "TABW"))
        goto fail;

    /* section0: base header */
    big_endian = (read_u16be(0x04,sf) == 0xFEFF); /* BOM (always LE on 3DS/Android) */
    if (big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }


    version = read_16bit(0x06, sf); /* assumed */
    if (version != 0x06 && version != 0x07) /* v6 = NFS: The Run , v7 = F&F Showndown */
        goto fail;

    total_subsongs = read_32bit(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    names_size = read_32bit(0x0c,sf);
    /* 0x10/14: see below */
    /* 0x18: data size (all subsongs) */
    offset = 0x1c + names_size; /* skip names table */

    /* the following 2 sections (rarely) won't match total_subsongs */

    /* section1: unknown */
    {
        size_t unknown_size = read_32bit(0x10,sf);
        /* 0x00: usually 0, rarely 0x20? */
        offset += unknown_size*0x04;
    }

    /* section2: samples */
    {
        size_t samples_size = read_32bit(0x14,sf);
        /* 0x00: num_samples */
        offset += samples_size*0x04;
    }

    /* section3: headers */
    {
        off_t header_offset = offset+(target_subsong-1)*0x24;

        name_offset = read_32bit(header_offset+0x00, sf) + 0x1c; /* within name table */
        codec = read_32bit(header_offset+0x04, sf);
        sample_rate = read_32bit(header_offset+0x08, sf);
        channel_count = read_32bit(header_offset+0x0c, sf);
        /* 0x10: index within section1/2? */
        /* 0x14: flags? 0x01 or (rarely) 0x02 */
        stream_offset = read_32bit(header_offset+0x18, sf);
        stream_size = read_32bit(header_offset+0x1c, sf); /* including DSP config */
        num_samples = read_32bit(header_offset+0x20, sf) / channel_count; /* nibble/PCMs */

        offset += total_subsongs*0x24;
    }

    loop_flag = 0;
    start_offset = offset + stream_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->meta_type = meta_WAVEBATCH;

    switch(codec) {
        case 0x00: /* PCM16 [NASCAR Unleashed (3DS), Solar Flux Pocket (Android)] */
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x01: /* PCM8 [Cars 2 (3DS)] */
            vgmstream->coding_type = coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;
            break;

        case 0x02: { /* DSP [WRC FIA World Rally Championship (3DS)] */
            size_t config_size = (0x20+0x14)*channel_count + (0x0c)*channel_count; /* coefs+hist + padding */

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (stream_size - config_size) / channel_count; /* full interleave*/;

            dsp_read_coefs(vgmstream,sf,start_offset+0x00,0x20+0x14,big_endian);
            dsp_read_hist (vgmstream,sf,start_offset+0x20,0x14+0x20,big_endian);
            start_offset += config_size;

            break;
        }

        default:
            VGM_LOG("WAVEBATCH: unknown codec %x\n", codec);
            goto fail;
    }

    read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf); /* always null-terminated */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
