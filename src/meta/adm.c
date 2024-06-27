#include "meta.h"
#include "../coding/coding.h"


typedef struct {
    int total_subsongs;
    int target_subsong;
    int file_version;
    int header_version; /* major.minor in hex */

    uint32_t stream_offset;
    uint32_t stream_size;

    int loop_flag;
    int sample_rate;
    int channels;
    int32_t num_samples;
} adm_header_t;

static int parse_adm(adm_header_t* adm, STREAMFILE* sf);

static VGMSTREAM* init_vgmstream_adm(STREAMFILE* sf, int file_version);

/* ADM2 - Crankcase Audio REV plugin file [The Grand Tour Game (PC)] */
VGMSTREAM* init_vgmstream_adm2(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "ADM2"))
        return NULL;
    if (!check_extensions(sf, "wem"))
        return NULL;

    return init_vgmstream_adm(sf, 2);
}

/* ADM3 - Crankcase Audio REV plugin file [Cyberpunk 2077 (PC), MotoGP 21 (PC)] */
VGMSTREAM* init_vgmstream_adm3(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "ADM3"))
        return NULL;
    if (!check_extensions(sf, "wem,bnk"))
        return NULL;

    return init_vgmstream_adm(sf, 3);
}

static VGMSTREAM* init_vgmstream_adm(STREAMFILE* sf, int file_version) {
    VGMSTREAM* vgmstream = NULL;
    adm_header_t adm = {0};

    /* ADMx are files used with the Wwise Crankaudio plugin, that simulate engine noises with
     * base internal samples and some internal RPM config (probably). Actual file seems to
     * define some combo of samples, this only plays those separate samples. */

    adm.target_subsong = sf->stream_index;
    if (adm.target_subsong == 0) adm.target_subsong = 1;

    adm.file_version = file_version;

    if (!parse_adm(&adm, sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(adm.channels, adm.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ADM;
    vgmstream->sample_rate = adm.sample_rate;
    vgmstream->num_samples = adm.num_samples; /* slightly lower than bytes-to-samples */
    vgmstream->num_streams = adm.total_subsongs;
    vgmstream->stream_size = adm.stream_size;

    switch(adm.header_version) {
        case 0x00070000: /* The Crew Motorfest (PC) */
            vgmstream->coding_type = coding_CRANKCASE_IMA;
            //vgmstream->layout_type = layout_interleave;
            //vgmstream->interleave_block_size = 0x23;
            break;

        default: /* The Grand Tour Game (PC) [0x00050000], MotoGP 21 (PC) [0x00060000] */
            /* Basically Apple's IMA (internally just "ADPCMDecoder") but transforms to float
            * each sample during decode by multiplying by 0.000030518509 */
            vgmstream->coding_type = coding_APPLE_IMA4;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x22;
            break;
    }

    if (!vgmstream_open_stream(vgmstream, sf, adm.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_type(adm_header_t* adm, STREAMFILE* sf, uint32_t offset) {

    /* ADM2 chunks */
    if (is_id32be(offset, sf, "GRN1") && adm->file_version == 2) {
        /* 0x74: offset to floats? */
        offset = read_u32le(offset + 0x78, sf);
        if (!parse_type(adm, sf, offset)) /* SMP1 */
            goto fail;
    }
    else if (is_id32be(offset, sf, "SMP1")) {
        adm->total_subsongs++;

        if (adm->target_subsong == adm->total_subsongs) {
            /* 0x04 always 0 */
            /* 0x08 version? (0x00030000) */
            adm->channels = read_u16le(offset + 0x0c, sf);
            /* 0x0e 0x0001? */
            /* 0x10 header size (0x2c) */
            adm->sample_rate = read_s32le(offset + 0x14, sf);
            adm->num_samples = read_s32le(offset + 0x18, sf);
            adm->stream_size = read_u32le(offset + 0x1c, sf);
            adm->stream_offset = read_u32le(offset + 0x20, sf);
            /* rest: null */
            VGM_LOG("so=%x %x\n", adm->stream_size, adm->stream_offset);
        }
    }

    /* ADM3 chunks */
    else if (is_id32be(offset, sf, "RMP1")) {
        off_t next_offset;
        next_offset = read_u32le(offset + 0x1c, sf);
        if (!parse_type(adm, sf, next_offset)) /* SMB1 */
            goto fail;
        next_offset = read_u32le(offset + 0x24, sf);
        if (!parse_type(adm, sf, next_offset)) /* GRN1 */
            goto fail;
    }
    else if (is_id32be(offset, sf, "GRN1") && adm->file_version == 3) {
        offset = read_u32le(offset + 0x5c, sf);
        if (!parse_type(adm, sf, offset)) /* SMP2 */
            goto fail;
    }
    else if (is_id32be(offset, sf, "SMB1")) {
        uint32_t table_count  = read_u32le(offset + 0x10, sf);
        uint32_t table_offset = read_u32le(offset + 0x18, sf);
        int i;

        for (i = 0; i < table_count; i++) {
            /* 16-bit value after smp2_unk rarely have non-zero values [PUBG Lite] */
            uint16_t smp2_unk    = read_u16le(table_offset + i * 0x08 + 0x00, sf);
            uint32_t smp2_offset = read_u32le(table_offset + i * 0x08 + 0x04, sf);

            if (smp2_unk != 1)
                goto fail;

            if (!parse_type(adm, sf, smp2_offset)) /* SMP2 */
                goto fail;
        }
    }
    else if (is_id32be(offset, sf, "SMP2")) {
        adm->total_subsongs++;

        if (adm->target_subsong == adm->total_subsongs) {
            /* 0x04 always 0 */
            /* 0x08 version? (0x00040000) */
            adm->channels = read_u32le(offset + 0x0c, sf); /* usually 4, with different sounds*/
            /* 0x10 float pitch? */
            /* 0x14 int pitch? */
            /* 0x18 0x0001? */
            /* 0x1a header size (0x30) */
            adm->sample_rate = read_s32le(offset + 0x1c, sf);
            adm->num_samples = read_s32le(offset + 0x20, sf);
            adm->stream_size = read_u32le(offset + 0x24, sf);
            /* 0x28 1? */
            adm->stream_offset = read_u32le(offset + 0x2c, sf);
        }
    }
    else {
        VGM_LOG("ADM: unknown at %x\n", offset);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static int parse_adm(adm_header_t* adm, STREAMFILE* sf) {
    uint32_t offset;

    /* 0x04: null */
    adm->header_version = read_u32le(0x08, sf); /* ADM2: 0x00050000, ADM3: 0x00060000 (older) / 0x00070000 (2023) */
    /* 0x0c: header size */
    /* 0x10: data start */
    /* rest unknown, looks mostly the same between files (some floats and stuff) */

    switch(adm->file_version) {
        case 2:
            /* low to high */
            offset = read_u32le(0x104, sf);
            if (!parse_type(adm, sf, offset)) goto fail; /* GRN1 */

            /* high to low */
            offset = read_u32le(0x108, sf);
            if (!parse_type(adm, sf, offset)) goto fail; /* GRN1 */

            /* idle engine */
            offset = read_u32le(0x10c, sf);
            if (!parse_type(adm, sf, offset)) goto fail; /* SMP1 */
            break;

        case 3:
            /* higher ramp, N samples from low to high */
            offset = read_u32le(0x0FC, sf);
            if (!parse_type(adm, sf, offset)) goto fail; /* RMP1 */
            if (read_u32le(0x100, sf) != 1) goto fail;

            /* lower ramp, also N samples */
            offset = read_u32le(0x104, sf);
            if (!parse_type(adm, sf, offset)) goto fail; /* RMP1 */
            if (read_u32le(0x108, sf) != 1) goto fail;

            /* idle engine */
            offset = read_u32le(0x10c, sf);
            if (offset != 0) { /* may not exist */
                if (!parse_type(adm, sf, offset)) goto fail; /* SMP2 */
                if (read_u32le(0x110, sf) != 1) goto fail;
            }
            break;

        default:
            goto fail;
    }

    if (adm->target_subsong < 0 || adm->target_subsong > adm->total_subsongs || adm->total_subsongs < 1)
        goto fail;

    return 1;
fail:
    return 0;
}
