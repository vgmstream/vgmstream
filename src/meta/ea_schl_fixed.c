#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* Possibly the same as EA_CODEC1_x in variable SCHl */
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
    int is_bank;

    off_t start_offset;
    //size_t stream_size;
    off_t loop_start;
    off_t loop_end;
} ea_fixed_header;

static VGMSTREAM* init_vgmstream_ea_fixed_header(STREAMFILE* sf, ea_fixed_header* ea);
static int parse_fixed_header(STREAMFILE* sf, ea_fixed_header* ea, off_t offset);


/* EA SCHl with fixed header - from EA games (~1997?) */
VGMSTREAM* init_vgmstream_ea_schl_fixed(STREAMFILE* sf) {
    ea_fixed_header ea = {0};
    size_t header_size;
    off_t offset;


    /* checks */
    if (!is_id32be(0x00, sf, "SCHl"))
        return NULL;

    /* .asf: original [NHK 97 (PC)]
     * .lasf: fake for plugins
     * .cnk/dct: ps1 [NBA Live 97 (PS1)] */
    if (!check_extensions(sf, "asf,lasf,cnk,dct"))
        return NULL;

    /* see ea_schl.c for more info about blocks */
    //TODO: handle SCCl? [NBA Live 97 (PS1)]

    header_size = read_u32le(0x04, sf);

    offset = is_id32be(0x0c, sf, "PATl") ? 0x0c : 0x08; /* extra field in PS1 */

    if (!parse_fixed_header(sf, &ea, offset))
        return NULL;

    ea.start_offset = header_size; /* has garbage data in PATl */
    return init_vgmstream_ea_fixed_header(sf, &ea);
}


/* EA BNKl with fixed header - from EA games (~1997?) */
VGMSTREAM* init_vgmstream_ea_bnk_fixed(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    ea_fixed_header ea = {0};
    int bnk_version, num_sounds;
    int total_subsongs = 0, target_subsong = sf->stream_index;
    off_t offset;


    /* checks */
    if (!is_id32be(0x00, sf, "BNKl"))
        return NULL;

    /* .bkh: FIFA 97 (PC)
     * .vh:  NBA Live 97 (PS1) */
    if (check_extensions(sf, "bkh")) {
        sf_body = open_streamfile_by_ext(sf, "bkd");
        if (!sf_body) goto fail;
    }
    else if (check_extensions(sf, "vh")) {
        sf_body = open_streamfile_by_ext(sf, "vb");
        if (!sf_body) goto fail;
    }
    else
        return NULL;


    ea.is_bank = 1;

    bnk_version = read_u16le(0x04, sf);
    num_sounds  = read_u16le(0x06, sf);

    /* (intentionally?) blanked fields [Triple Play 97 (PC)] */
    if (!bnk_version && !num_sounds) {
        bnk_version = 0x01;
        num_sounds  = 0x80;
    }

    /* bnk v2+ is only in standard (variable header) ea_schl */
    if (bnk_version != 0x01) goto fail;
    /* always a 0x200 sized buffer w/ dummy nullptr entries */
    if (num_sounds != 0x80) goto fail;

    if (target_subsong == 0) target_subsong = 1;

    for (int i = 0; i < num_sounds; i++) {
        offset = read_u32le(0x08 + (i * 0x04), sf);
        if (!offset) continue;
        total_subsongs++;

        if (total_subsongs == target_subsong) {
            offset += 0x08 + (i * 0x04); /* pointer base is ptr pos */
            if (!parse_fixed_header(sf, &ea, offset))
                goto fail;
        }
    }

    if (total_subsongs < 1 || target_subsong > total_subsongs)
        goto fail;

    vgmstream = init_vgmstream_ea_fixed_header(sf_body, &ea);
    if (!vgmstream) goto fail;
    vgmstream->num_streams = total_subsongs;


    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}


/* EA standalone PATl fixed header - from EA games (~1997?) */
VGMSTREAM* init_vgmstream_ea_patl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    ea_fixed_header ea = {0};


    /* checks */
    if (!parse_fixed_header(sf, &ea, 0x00))
        return NULL;

    /* .pth: Triple Play 97 (PC) */
    /* often also found as nameless file pairs in bigfiles [FIFA 97 (PC)] */
    if (check_extensions(sf, "pth")) {
        sf_body = open_streamfile_by_ext(sf, "ptd");
        if (!sf_body) goto fail;
    }
    else
        return NULL;


    ea.is_bank = 1;
    if (ea.start_offset != 0x00) goto fail;

    vgmstream = init_vgmstream_ea_fixed_header(sf_body, &ea);
    if (!vgmstream) goto fail;


    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}


static VGMSTREAM* init_vgmstream_ea_fixed_header(STREAMFILE* sf, ea_fixed_header* ea) {
    VGMSTREAM* vgmstream = NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea->channels, ea->loop_flag);
    if (!vgmstream) goto fail;

    //vgmstream->stream_size = ea->stream_size; /* only on PS1, and sometimes uninitialised garbage? */
    vgmstream->sample_rate = ea->sample_rate;
    vgmstream->num_samples = ea->num_samples;
    /* loops are rare, loop_end might also need +1 just like variable_header?
     * NBA Live 97 (PS1, JPN) ZAUDIOFX.BKH #15-#18 loop_end == num_samples-1 */
    vgmstream->loop_start_sample = ea->loop_start;
    vgmstream->loop_end_sample = ea->loop_end;

    vgmstream->codec_endian = ea->big_endian;

    vgmstream->meta_type = ea->is_bank ? meta_EA_BNK_fixed : meta_EA_SCHL_fixed;
    vgmstream->layout_type = ea->is_bank ? layout_none : layout_blocked_ea_schl;

    switch (ea->codec) {
        case EA_CODEC_PCM:
            vgmstream->coding_type = ea->bps == 8 ? coding_PCM8 : (ea->big_endian ? coding_PCM16BE : coding_PCM16LE);
            break;

        case EA_CODEC_IMA:
            vgmstream->coding_type = coding_DVI_IMA; /* stereo/mono, high nibble first */
            break;

        case EA_CODEC_PSX:
            vgmstream->coding_type = coding_PSX;
            break;

        default:
            VGM_LOG("EA SCHl: unknown fixed header codec 0x%02x\n", ea->codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, ea->start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_fixed_header(STREAMFILE* sf, ea_fixed_header* ea, off_t offset) {

    if (!is_id32be(offset + 0x00, sf, "PATl"))
        goto fail;

    /* PATl header is 0x10 bytes, and can be separate from the rest of the body (BNKl) */
    offset += read_u32le(offset + 0x0c, sf) + 0x0c; /* body pointer base from ptr pos */

    //size = read_u32le(offset + 0x34, sf); /* pointer to end of all header bodies, not size */
    if (/*size == 0x20 &&*/ is_id32be(offset + 0x28, sf, "TMpl")) { /* PC LE? */
        offset += 0x2c;

        ea->version      = read_u8   (offset + 0x00, sf);
        ea->bps          = read_u8   (offset + 0x01, sf);
        ea->channels     = read_u8   (offset + 0x02, sf);
        ea->codec        = read_u8   (offset + 0x03, sf);
        /* 0x04: 0? */
        ea->sample_rate  = read_u16le(offset + 0x06, sf);
        ea->num_samples  = read_s32le(offset + 0x08, sf);
        ea->loop_start   = read_s32le(offset + 0x0c, sf);
        ea->loop_end     = read_s32le(offset + 0x10, sf);
        ea->start_offset = read_u32le(offset + 0x14, sf); /* BNKl only */
        /* 0x18: -1? */ /* SCHl only */
    }
    else if (/*size == 0x38 &&*/ is_id32be(offset + 0x28, sf, "TMxl")) { /* PSX LE? */
        offset += 0x2c;

        ea->version      = read_u8   (offset + 0x00, sf);
        ea->bps          = read_u8   (offset + 0x01, sf);
        ea->channels     = read_u8   (offset + 0x02, sf);
        ea->codec        = read_u8   (offset + 0x03, sf);
        /* 0x04: 0? */
        ea->sample_rate  = read_u16le(offset + 0x06, sf);
        /* 0x08: 0x20C in SCHl, 0x800 in BNKl? */
        ea->num_samples  = read_s32le(offset + 0x0c, sf);
        ea->loop_start   = read_s32le(offset + 0x10, sf);
        ea->loop_end     = read_s32le(offset + 0x14, sf);
        ea->start_offset = read_u32le(offset + 0x18, sf); /* BNKl only */
        /* 0x1c: 0? */
        //ea->stream_size  = read_u32le(offset + 0x20, sf); /* BNKl only */
        /* 0x24: 0? */
        /* 0x28: -1? */
        /* 0x2c: -1? */
        /* 0x30: -1? */ /* SCHl only */
    }
    else {
        VGM_LOG("EA SCHl: unknown fixed header subtype\n");
        goto fail;
    }
    /* after header bodies */
    /* 0x00: volume? (127 or 128) */

    ea->loop_flag = (ea->loop_end != -1);

    return 1;
fail:
    return 0;
}
