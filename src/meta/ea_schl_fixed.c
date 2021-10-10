#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* Possibly the same as EA_CODEC_x in variable SCHl */
#define EA_CODEC_PCM            0x00
#define EA_CODEC_IMA            0x02
#define EA_CODEC_PSX            0x06

typedef struct {
    int8_t version;
    int8_t bps;
    int8_t channels;
    int8_t codec;
    int sample_rate;
    int32_t num_samples;

    int big_endian;
    int loop_flag;
} ea_fixed_header;

static int parse_fixed_header(STREAMFILE* sf, ea_fixed_header* ea);


/* EA SCHl with fixed header - from EA games (~1997?) */
VGMSTREAM* init_vgmstream_ea_schl_fixed(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t header_size;
    ea_fixed_header ea = {0};


    /* checks */
    if (!is_id32be(0x00,sf, "SCHl"))
        goto fail;

    /* .asf: original [NHK 97 (PC)]
     * .lasf: fake for plugins
     * .cnk: ps1 [NBA Live 97 (PS1)] */
    if (!check_extensions(sf,"asf,lasf,cnk"))
        goto fail;

    /* see ea_schl.c for more info about blocks */
    //TODO: handle SCCl? [NBA Live 97 (PS1)]

    header_size = read_u32le(0x04,sf);

    if (!parse_fixed_header(sf, &ea))
        goto fail;

    start_offset = header_size;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea.channels, ea.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ea.sample_rate;
    vgmstream->num_samples = ea.num_samples;
    //vgmstream->loop_start_sample = ea.loop_start;
    //vgmstream->loop_end_sample = ea.loop_end;

    vgmstream->codec_endian = ea.big_endian;

    vgmstream->meta_type = meta_EA_SCHL_fixed;
    vgmstream->layout_type = layout_blocked_ea_schl;

    switch (ea.codec) {
        case EA_CODEC_PCM:
            vgmstream->coding_type = ea.bps==8 ? coding_PCM8 : (ea.big_endian ? coding_PCM16BE : coding_PCM16LE);
            break;

        case EA_CODEC_IMA:
            vgmstream->coding_type = coding_DVI_IMA; /* stereo/mono, high nibble first */
            break;

        case EA_CODEC_PSX:
            vgmstream->coding_type = coding_PSX;
            break;

        default:
            VGM_LOG("EA: unknown codec 0x%02x\n", ea.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_fixed_header(STREAMFILE* sf, ea_fixed_header* ea) {
    uint32_t offset = 0x00, size = 0;

    if (is_id32be(offset+0x08, sf, "PATl"))
        offset = 0x08;
    else if (is_id32be(offset+0x0c, sf, "PATl"))
        offset = 0x0c; /* extra field in PS1 */
    else
        goto fail;

    size = read_u32le(offset+0x34, sf);
    if (size == 0x20 && is_id32be(offset+0x38, sf, "TMpl")) { /* PC LE? */
        offset += 0x3c;

        ea->version     = read_u8   (offset+0x00, sf);
        ea->bps         = read_u8   (offset+0x01, sf);
        ea->channels    = read_u8   (offset+0x02, sf);
        ea->codec       = read_u8   (offset+0x03, sf);
        /* 0x04: 0? */
        ea->sample_rate = read_u16le(offset+0x06, sf);
        ea->num_samples = read_s32le(offset+0x08, sf);
        /* 0x0c: -1? loop_start? */
        /* 0x10: -1? loop_end? */
        /* 0x14: 0? data start? */
        /* 0x18: -1? */
        /* 0x1c: volume? (always 128) */
    }
    else if (size == 0x38 && is_id32be(offset+0x38, sf, "TMxl")) { /* PSX LE? */
        offset += 0x3c;

        ea->version     = read_u8   (offset+0x00, sf);
        ea->bps         = read_u8   (offset+0x01, sf);
        ea->channels    = read_u8   (offset+0x02, sf);
        ea->codec       = read_u8   (offset+0x03, sf);
        /* 0x04: 0? */
        ea->sample_rate = read_u16le(offset+0x06, sf);
        /* 0x08: 0x20C? */
        ea->num_samples = read_s32le(offset+0x0c, sf);
        /* 0x10: -1? loop_start? */
        /* 0x14: -1? loop_end? */
        /* 0x18: 0x20C? */
        /* 0x1c: 0? */
        /* 0x20: 0? */
        /* 0x24: 0? */
        /* 0x28: -1? */
        /* 0x2c: -1? */
        /* 0x30: -1? */
        /* 0x34: volume? (always 128) */
    }
    else {
        goto fail;
    }

    //ea->loop_flag = (ea->loop_end_sample);

    return 1;
fail:
    return 0;
}
