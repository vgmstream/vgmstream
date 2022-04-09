#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/endianness.h"

#define EA_CODEC_PCM            0x00
#define EA_CODEC_ULAW           0x01
#define EA_CODEC_IMA            0x02
#define EA_CODEC_PSX            0xFF //fake value

typedef struct {
    int32_t sample_rate;
    uint8_t bits;
    uint8_t channels;
    uint8_t codec;
    uint8_t type;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int32_t loop_start_offset;
    uint32_t data_offset;

    uint32_t base_size;

    int big_endian;
    int loop_flag;
    int is_sead;
    int codec_config;
    int is_bank;
    int total_subsongs;
} eacs_header;

static int parse_header(STREAMFILE* sf, eacs_header* ea, uint32_t begin_offset);
static VGMSTREAM* init_vgmstream_main(STREAMFILE* sf, eacs_header* ea);

static void set_ea_1snh_num_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, eacs_header* ea, int find_loop);
static int get_ea_1snh_ima_version(STREAMFILE* sf, off_t start_offset, const eacs_header* ea);

/* EA 1SNh - from early EA games, stream (~1996, ex. Need for Speed) */
VGMSTREAM* init_vgmstream_ea_1snh(STREAMFILE* sf) {
    eacs_header ea = {0};
    off_t offset = 0x00, eacs_offset;
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    /* in TGV videos, either TGVk or 1SNh block comes first */
    if (is_id32be(0x00, sf, "TGVk")) {
        offset = read_u32be(0x04, sf);
    } else if (is_id32be(0x00, sf, "kVGT")) {
        offset = read_u32le(0x04, sf);
    }

    if (!is_id32be(offset + 0x00, sf, "1SNh") &&
        !is_id32be(offset + 0x00, sf, "SEAD"))
        goto fail;

    /* .asf/as4: common,
     * .lasf: fake for plugins
     * .sng: fake for plugins (for .asf issues)
     * .cnk: some PS1 games [Triple Play 97 (PS1), FIFA 97 (PS1)]
     * .uv/tgq: some SAT videos
     * .tgv: videos
     * (extensionless): Need for Speed (SAT) videos */
    if (!check_extensions(sf, "asf,lasf,sng,as4,cnk,uv,tgq,tgv,"))
        goto fail;

    /* stream is divided into blocks/chunks: 1SNh=audio header, 1SNd=data xN, 1SNl=loop end, 1SNe=end.
     * Video uses various blocks (TGVk/TGVf/MUVf/etc) and sometimes alt audio blocks (SEAD/SNDC/SEND). */
    ea.is_sead = is_id32be(offset + 0x00, sf, "SEAD");

    if (!ea.is_sead)
        ea.base_size = read_u32le(offset + 0x04, sf);

    eacs_offset = offset + 0x08; /* after 1SNh block id+size */

    if (!parse_header(sf, &ea, eacs_offset))
        goto fail;

    vgmstream = init_vgmstream_main(sf, &ea);
    if (!vgmstream) goto fail;

    if (ea.num_samples == 0) {
        /* header does not specify number of samples, need to calculate it manually */
        /* HACK: we need vgmstream object to use blocked layout so we're doing this calc after creating it */
        set_ea_1snh_num_samples(vgmstream, sf, &ea, 0);

        /* update samples and loop state */
        vgmstream->num_samples = ea.num_samples;
        vgmstream_force_loop(vgmstream, ea.loop_flag, ea.loop_start, ea.loop_end);
    }

    return vgmstream;

fail:
    return NULL;
}

/* EA EACS - from early EA games, bank (~1996, ex. Need for Speed) */
VGMSTREAM* init_vgmstream_ea_eacs(STREAMFILE* sf) {
    eacs_header ea = {0};
    off_t eacs_offset;


    /* checks */
    if (!is_id32be(0x00,sf, "EACS") &&
        read_u32be(0x00,sf) != 0 && !is_id32be(0x228,sf, "EACS"))
        goto fail;

    /* .eas: single bank [Need for Speed (PC)]
     * .bnk: multi bank [Need for Speed (PC)]
     * .as4: single [NBA Live 96 (PC)] */
    if (!check_extensions(sf,"eas,bnk,as4"))
        goto fail;

    /* plain data without blocks, can contain N*(EACS header) + N*(data), or N (EACS header + data) */
    ea.is_bank = 1;

    if (is_id32be(0x00,sf, "EACS")) {
        /* single bank variant */
        eacs_offset = 0x00;
    }
    else if (read_u32be(0x00,sf) == 0x00) {
        /* multi bank variant */
        int i;
        int target_subsong = sf->stream_index;

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0) goto fail;

        /* offsets to EACSs are scattered in the first 0x200, then 0x28 info + EACS per subsong. 
         * This looks dumb but seems like the only way. */
        eacs_offset = 0;
        for (i = 0x00; i < 0x200; i += 0x04) {
            off_t bank_offset = read_u32le(i, sf);
            if (bank_offset == 0)
                continue;

            ea.total_subsongs++;

            /* parse mini bank header */
            if (ea.total_subsongs == target_subsong) {
                /* 0x00: some id or flags? */
                eacs_offset = read_u32le(bank_offset + 0x04, sf); /* always after 0x28 from bank_offset */
                if (!is_id32be(eacs_offset, sf, "EACS"))
                    goto fail;
                /* rest: not sure if part of this header */
            }
        }

        if (eacs_offset == 0)
            goto fail;
    }
    else {
        goto fail;
    }

    if (!parse_header(sf,&ea, eacs_offset))
        goto fail;
    return init_vgmstream_main(sf, &ea);

fail:
    return NULL;
}


static VGMSTREAM* init_vgmstream_main(STREAMFILE* sf, eacs_header* ea) {
    VGMSTREAM* vgmstream = NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea->channels, ea->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ea->sample_rate;
    vgmstream->num_samples = ea->num_samples;
    vgmstream->loop_start_sample = ea->loop_start;
    vgmstream->loop_end_sample = ea->loop_end;

    vgmstream->codec_endian = ea->big_endian;
    vgmstream->layout_type = ea->is_bank ? layout_none : layout_blocked_ea_1snh;
    vgmstream->meta_type = ea->is_bank ? meta_EA_EACS : meta_EA_1SNH;
    vgmstream->num_streams = ea->total_subsongs;

    switch (ea->codec) {
        case EA_CODEC_PCM: /* Need for Speed (PC) */
            vgmstream->coding_type = ea->bits==1 ? coding_PCM8_int : coding_PCM16_int;
            break;

        case EA_CODEC_ULAW: /* Crusader: No Remorse movies (SAT), FIFA 96 movies (SAT) */
            if (ea->bits && ea->bits != 2) goto fail; /* only set in EACS */
            vgmstream->coding_type = coding_ULAW_int;
            break;

        case EA_CODEC_IMA: /* Need for Speed (PC) */
            if (ea->bits && ea->bits != 2) goto fail; /* only in EACS */
            vgmstream->coding_type = coding_DVI_IMA; /* stereo/mono, high nibble first */
            vgmstream->codec_config = ea->codec_config;
            break;

        case EA_CODEC_PSX: /* Need for Speed (PS1) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->codec_config = ea->codec_config;
            break;

        default:
            vgm_logi("EA EACS: unknown codec 0x%02x\n", ea->codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, ea->data_offset))
        goto fail;

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

static int parse_header(STREAMFILE* sf, eacs_header* ea, uint32_t offset) {
    /* audio header endianness doesn't always match block headers, use sample rate to detect */
    read_s32_t read_s32;

    if (is_id32be(offset+0x00,sf, "EACS")) {
        /* EACS subheader (PC, SAT) */
        ea->big_endian = guess_endian32(offset + 0x04, sf);
        read_s32 = ea->big_endian ? read_s32be : read_s32le;

        ea->sample_rate = read_s32(offset+0x04, sf);
        ea->bits        =  read_u8(offset+0x08, sf);
        ea->channels    =  read_u8(offset+0x09, sf);
        ea->codec       =  read_u8(offset+0x0a, sf);
        ea->type        =  read_u8(offset+0x0b, sf); /* block type? 0=1SNh, -1=bank */
        ea->num_samples = read_s32(offset+0x0c, sf);
        ea->loop_start  = read_s32(offset+0x10, sf);
        ea->loop_end    = read_s32(offset+0x14, sf) + ea->loop_start; /* loop length */
        ea->data_offset = read_s32(offset+0x18, sf); /* 0 when blocked, usually */
        /* 0x1c: pan/volume/etc? (0x7F)
         * rest may be padding/garbage */
        //VGM_ASSERT(ea->type != 0, "EA EACS: unknown type %i\n", ea->type);

        /* blocked should set 0 but in rare cases points to data start [NBA Live 95 (MS-DOS)] */
        if (!ea->is_bank)
            ea->data_offset = 0;

        if (ea->codec == EA_CODEC_IMA)
            ea->codec_config = get_ea_1snh_ima_version(sf, 0x00, ea);
        /* EACS banks with empty values exist but will be rejected later */
    }
    else if (ea->is_sead) {
        /* alt subheader (found in some PC videos) */
        ea->big_endian = guess_endian32(offset + 0x00, sf);
        read_s32 = ea->big_endian ? read_s32be : read_s32le;

        ea->sample_rate = read_s32(offset+0x00, sf);
        ea->channels    = read_s32(offset+0x04, sf);
        ea->codec       = read_s32(offset+0x08, sf);

        if (ea->codec == EA_CODEC_IMA)
            ea->codec_config = get_ea_1snh_ima_version(sf, 0x00, ea);
    }
    else if (ea->base_size == 0x2c) {
        /* [NBA Live 96 (PS1), Need for Speed (PS1)] */
        ea->sample_rate = read_s32le(offset+0x00, sf);
        ea->channels    =  read_u8(offset+0x18, sf);
        ea->codec       = EA_CODEC_PSX;
        ea->codec_config = 0;
    }
    else if (ea->base_size == 0x30) {
        /* [FIFA 97 (PS1), Triple Play 97 (PS1)] */
        /* 0x00: 0 or some id? (same for N files) */
        ea->sample_rate = read_s32le(offset+0x04, sf);
        ea->channels    =  read_u8(offset+0x1c, sf);
        ea->codec       = EA_CODEC_PSX;
        ea->codec_config = 1;
    }
    else {
        //TODO: test
        /* found in early videos, similar to EACS */
        ea->big_endian = guess_endian32(offset + 0x04, sf);
        read_s32 = ea->big_endian ? read_s32be : read_s32le;

        ea->sample_rate = read_s32(offset + 0x04, sf);
        ea->bits        =  read_u8(offset + 0x08, sf);
        ea->channels    =  read_u8(offset + 0x09, sf);
        ea->codec       =  read_u8(offset + 0x0a, sf);
        ea->type        =  read_u8(offset + 0x0b, sf); /* block type? 0=1SNh, -1=bank */

        if (ea->codec == EA_CODEC_IMA)
            ea->codec_config = get_ea_1snh_ima_version(sf, 0x00, ea);
    }

    ea->loop_flag = (ea->loop_end > 0);

    return 1;
}

/* get total samples by parsing block headers, needed when EACS isn't present */
static void set_ea_1snh_num_samples(VGMSTREAM *vgmstream, STREAMFILE* sf, eacs_header* ea, int find_loop) {
    int32_t num_samples = 0, block_id;
    size_t file_size;
    read_s32_t read_s32 = ea->big_endian ? read_s32be : read_s32le;
    int loop_end_found = 0;

    file_size = get_streamfile_size(sf);
    vgmstream->next_block_offset = ea->data_offset;

    while (vgmstream->next_block_offset < file_size) {
        block_update(vgmstream->next_block_offset, vgmstream);
        if (vgmstream->current_block_samples < 0)
            break;

        block_id = read_u32be(vgmstream->current_block_offset, sf);

        if (find_loop) {
            if (vgmstream->current_block_offset == ea->loop_start_offset) {
                ea->loop_start = num_samples;
                ea->loop_flag = 1;
                block_update(ea->data_offset, vgmstream);
                return;
            }
        }
        else {
            if (block_id == get_id32be("1SNl") ) {  /* loop point found */
                ea->loop_start_offset = read_s32(vgmstream->current_block_offset + 0x08, sf);
                ea->loop_end = num_samples;
                loop_end_found = 1;
            }
        }

        num_samples += vgmstream->current_block_samples;
    }

    ea->num_samples = num_samples;

    /* reset once we're done */
    block_update(ea->data_offset, vgmstream);

    if (loop_end_found) {
        /* recurse to find loop start sample */
        set_ea_1snh_num_samples(vgmstream, sf, ea, 1);
    }
}

/* find codec version used, with or without ADPCM hist per block */
static int get_ea_1snh_ima_version(STREAMFILE* sf, off_t start_offset, const eacs_header* ea) {
    off_t block_offset = start_offset;
    size_t file_size = get_streamfile_size(sf);
    read_s32_t read_s32 = ea->big_endian ? read_s32be : read_s32le;

    if (ea->type == 0xFF) /* bnk */
        return 0;

    while (block_offset < file_size) {
        uint32_t id = read_u32be(block_offset+0x00,sf);
        size_t block_size;

        /* BE in SAT, but one file may have both BE and LE chunks */
        if (guess_endian32(block_offset + 0x04, sf))
            block_size = read_u32be(block_offset + 0x04, sf);
        else
            block_size = read_u32le(block_offset + 0x04, sf);

        if (block_size == 0 || block_size == -1)
            break;

        if (id == get_id32be("1SNd") || id == get_id32be("SNDC")) {
            int32_t ima_samples = read_s32(block_offset + 0x08, sf);
            int32_t expected_samples = (block_size - 0x08 - 0x04 - 0x08*ea->channels) * 2 / ea->channels;

            if (ima_samples == expected_samples) {
                return 1; /* has ADPCM hist (hopefully) */
            }
        }

        block_offset += block_size;
    }

    return 0; /* no ADPCM hist */
}
