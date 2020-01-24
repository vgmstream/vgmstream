#include "meta.h"
#include "../coding/coding.h"

typedef enum { PCM16, MSADPCM, DSP, AT9 } kwb_codec;

typedef struct {
    int total_subsongs;
    int target_subsong;
    kwb_codec codec;

    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int loop_flag;
    int block_size;

    off_t stream_offset;
    size_t stream_size;

    off_t dsp_offset;
    //off_t name_offset;
} kwb_header;

static int parse_kwb(kwb_header* kwb, STREAMFILE* sf_h, STREAMFILE* sf_b);


/* KWB - WaveBank from Koei games */
VGMSTREAM * init_vgmstream_kwb(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *sf_h = NULL, *sf_b = NULL;
    kwb_header kwb = {0};
    int target_subsong = sf->stream_index;


    /* checks */
    /* .wbd+wbh: common [Bladestorm Nightmare (PC)]
     * .wb2+wh2: newer [Nights of Azure 2 (PC)]
     * .sed: mixed header+data [Dissidia NT (PC)] */
    if (!check_extensions(sf, "wbd,wb2,sed"))
        goto fail;


    /* open companion header */
    if (check_extensions(sf, "wbd")) {
        sf_h = open_streamfile_by_ext(sf, "wbh");
        sf_b = sf;
    }
    else if (check_extensions(sf, "wb2")) {
        sf_h = open_streamfile_by_ext(sf, "wh2");
        sf_b = sf;
    }
    else if (check_extensions(sf, "sed")) {
        sf_h = sf;
        sf_b = sf;
    }
    else {
        goto fail;
    }

    if (sf_h == NULL || sf_b == NULL)
        goto fail;

    if (target_subsong == 0) target_subsong = 1;
    kwb.target_subsong = target_subsong;

    if (!parse_kwb(&kwb, sf_h, sf_b))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(kwb.channels, kwb.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KWB;
    vgmstream->sample_rate = kwb.sample_rate;
    vgmstream->num_samples = kwb.num_samples;
    vgmstream->stream_size = kwb.stream_size;
    vgmstream->num_streams = kwb.total_subsongs;

    switch(kwb.codec) {
        case PCM16: /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case MSADPCM:
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = kwb.block_size;
            break;

        case DSP:
            if (kwb.channels > 1) goto fail;
            vgmstream->coding_type = coding_NGC_DSP; /* subinterleave? */
            vgmstream->layout_type = layout_interleave;
            vgmstream->layout_type = 0x08;
            dsp_read_coefs_le(vgmstream, sf_h, kwb.dsp_offset + 0x1c, 0x60);
            dsp_read_hist_le (vgmstream, sf_h, kwb.dsp_offset + 0x40, 0x60);
            break;

#ifdef VGM_USE_ATRAC9
        case AT9: {
            atrac9_config cfg = {0};

            {
                size_t extra_size = read_u32le(kwb.stream_offset + 0x00, sf_b);
                uint32_t config_data = read_u32be(kwb.stream_offset + 0x04, sf_b);
                /* 0x0c: encoder delay? */
                /* 0x0e: encoder padding? */
                /* 0x10: samples per frame */
                /* 0x12: frame size */

                cfg.channels = vgmstream->channels;
                cfg.config_data = config_data;

                kwb.stream_offset += extra_size;
                kwb.stream_size -= extra_size;
            }

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            //TODO: check encoder delay
            vgmstream->num_samples = atrac9_bytes_to_samples_cfg(kwb.stream_size, cfg.config_data);
            break;
        }
#endif


        default:
            goto fail;
    }

    if (sf_h != sf) close_streamfile(sf_h);

    if (!vgmstream_open_stream(vgmstream, sf_b, kwb.stream_offset))
        goto fail;
    return vgmstream;

fail:
    if (sf_h != sf) close_streamfile(sf_h);
    close_vgmstream(vgmstream);
    return NULL;
}

static int parse_type_kwb2(kwb_header* kwb, off_t offset, STREAMFILE* sf_h) {
    int i, j, sounds;

    /* 00: KWB2/KWBN id */
    /* 04: always 0x3200? */
    sounds = read_u16le(offset + 0x06, sf_h);
    /* 08: ? */
    /* 0c: 1.0? */
    /* 10: null or 1 */
    /* 14: offset to HDDB table (from type), can be null */

    /* offset table to entries */
    for (i = 0; i < sounds; i++) {
        off_t sound_offset = read_u32le(offset + 0x18 + i*0x04, sf_h);
        int subsounds, subsound_start, subsound_size;
        uint16_t version;


        if (sound_offset == 0) /* common... */
            continue;
        sound_offset += offset;

        /* sound entry */
        version = read_u16le(sound_offset + 0x00, sf_h);
        /* 00: version? */
        /* 02: 0x2b or 0x32 */
        subsounds = read_u8(sound_offset + 0x03, sf_h);
        /* 03: subsounds? */
        /* others: unknown or null */

        /* unsure but seems to work, maybe upper byte only */
        if (version < 0xc000) {
            subsound_start = 0x2c;
            subsound_size  = 0x48;
        }
        else {
            subsound_start = read_u16le(sound_offset + 0x2c, sf_h);
            subsound_size  = read_u16le(sound_offset + 0x2e, sf_h);
        }

        subsound_start = sound_offset + subsound_start;

        for (j = 0; j < subsounds; j++) {
            off_t subsound_offset;
            uint8_t codec;

            kwb->total_subsongs++;
            if (kwb->total_subsongs != kwb->target_subsong)
                continue;
            subsound_offset = subsound_start + j*subsound_size;

            kwb->sample_rate    = read_u16le(subsound_offset + 0x00, sf_h);
            codec               = read_u8   (subsound_offset + 0x02, sf_h);
            kwb->channels       = read_u8   (subsound_offset + 0x03, sf_h);
            kwb->block_size     = read_u16le(subsound_offset + 0x04, sf_h);
            /* 0x06: samples per frame in MSADPCM? */
            /* 0x08: some id? (not always) */
            kwb->num_samples    = read_u32le(subsound_offset + 0x0c, sf_h);
            kwb->stream_offset  = read_u32le(subsound_offset + 0x10, sf_h);
            kwb->stream_size    = read_u32le(subsound_offset + 0x14, sf_h);
            /* when size > 0x48 */
            /* 0x48: subsound entry size */
            /* rest: reserved per codec? (usually null) */

            switch(codec) {
                case 0x00:
                    kwb->codec = PCM16;
                    break;
                case 0x10:
                    kwb->codec = MSADPCM;
                    break;
                case 0x90:
                    kwb->codec = DSP;
                    kwb->dsp_offset = subsound_offset + 0x4c;
                    break;
                default:
                    VGM_LOG("KWB2: unknown codec\n");
                    goto fail;
            }
        }
    }

    //TODO: read names
    /* HDDB table (optional and not too common)
    00 HDDB id
    04 1?
    08: 20? start?
    0c: 14? start?
    10: size
    14: name table start
    20: name offsets?
    then some subtable
    then name table (null terminated and one after other)
    */

    if (kwb->target_subsong < 0 || kwb->target_subsong > kwb->total_subsongs || kwb->total_subsongs < 1) goto fail;

    return 1;
fail:
    return 0;
}

static int parse_type_k4hd(kwb_header* kwb, off_t offset, STREAMFILE* sf_h) {
    off_t ppva_offset, header_offset;
    int entries;
    size_t entry_size;


    /* a format mimicking PSVita's hd4+bd4 format */
    /* 00 K4HD id */
    /* 04 chunk size */
    /* 08: ? */
    /* 0c: ? */
    /* 10: PPPG offset ('program'? cues?) */
    /* 14: PPTN offset ('tone'? sounds?) */
    /* 18: PPVA offset ('VAG'? waves) */
    ppva_offset = read_u16le(offset + 0x18, sf_h);
    ppva_offset += offset;

    /* PPVA table: */
    if (read_u32be(ppva_offset + 0x00, sf_h) != 0x50505641) /* "PPVA" */
        goto fail;

    entry_size = read_u32le(ppva_offset + 0x08, sf_h); /* */
    /* 0x0c: -1? */
    /* 0x10: 0? */
    entries = read_u32le(ppva_offset + 0x14, sf_h) + 1;
    /* 0x18: -1? */
    /* 0x1c: -1? */

    if (entry_size != 0x1c) {
        VGM_LOG("K4HD: unknown entry size\n");
        goto fail;
    }

    kwb->total_subsongs = entries;
    if (kwb->target_subsong < 0 || kwb->target_subsong > kwb->total_subsongs || kwb->total_subsongs < 1) goto fail;

    header_offset = ppva_offset + 0x20 + (kwb->target_subsong-1) * entry_size;

    kwb->stream_offset  = read_u32le(header_offset + 0x00, sf_h);
    kwb->sample_rate    = read_u32le(header_offset + 0x04, sf_h);
    kwb->stream_size    = read_u32le(header_offset + 0x08, sf_h);
    /* 0x0c: -1? loop? */
    if (read_u32le(header_offset + 0x10, sf_h) != 2) { /* codec? */
        VGM_LOG("K4HD: unknown codec\n");
        goto fail;
    }
    /* 0x14: loop start? */
    /* 0x18: loop end? */

    kwb->codec = AT9;
    kwb->channels = 1; /* always, devs use dual subsongs to fake stereo (like as hd3+bd3) */


    return 1;
fail:
    return 0;
}

static int parse_type_sdsd(kwb_header* kwb, off_t offset, STREAMFILE* sf_h) {
    /* has Vers, Head, Prog, Smpl sections (like Sony VABs)
    unknown codec, blocked with some common start, variable sized */
    return 0;
}

static int parse_kwb(kwb_header* kwb, STREAMFILE* sf_h, STREAMFILE* sf_b) {
    off_t head_offset, body_offset, start;
    uint32_t type;


    if (read_u32be(0x00, sf_h) == 0x57484431) { /* "WHD1" */
        /* container of wbh+wbd */
        /* 0x04: fixed value? */
        /* 0x08: version? */
        start = read_u32le(0x0c, sf_h);
        /* 0x10: file size */
        /* 0x14: subfiles? */
        /* 0x18: subfiles? */
        /* 0x1c: null */
        /* 0x20: some size? */
        /* 0x24: some size? */

        head_offset = read_u32le(start + 0x00, sf_h);
        body_offset = read_u32le(start + 0x04, sf_h);
        /* 0x10: head size */
        /* 0x14: body size */
    }
    else {
        /* dual file */
        head_offset = 0x00;
        body_offset = 0x00;
    }

    if (read_u32be(head_offset + 0x00, sf_h) != 0x5F484257 ||   /* "_HBW" */
        read_u32be(head_offset + 0x04, sf_h) != 0x30303030)     /* "0000" */
        goto fail;
    if (read_u32be(body_offset + 0x00, sf_b) != 0x5F444257 ||   /* "_DBW" */
        read_u32be(body_offset + 0x04, sf_b) != 0x30303030)     /* "0000" */
        goto fail;
    /* 0x08: head/body size */

    head_offset += 0x0c;
    body_offset += 0x0c;

    /* format has multiple bank subtypes that are quite different from each other */
    type = read_u32be(head_offset + 0x00, sf_h);
    switch(type) {
        case 0x4B574232: /* "KWB2" (PC) */
        case 0x4B57424E: /* "KWBN" (Switch) */
            if (!parse_type_kwb2(kwb, head_offset, sf_h))
                goto fail;
            break;

        case 0x4B344844: /* "K4HD" (PS4/Vita) */
            if (!parse_type_k4hd(kwb, head_offset, sf_h))
                goto fail;
            break;

        case 0x53447364: /* "SDsd" (PS3?) */
            if (!parse_type_sdsd(kwb, head_offset, sf_h))
                goto fail;
            break;

        default:
            goto fail;
    }

    kwb->stream_offset += body_offset;

    return 1;
fail:
    return 0;
}
