#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

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
    int32_t data_offset;

    int big_endian;
    int loop_flag;
    int is_sead;
    int codec_config;
    int is_bank;
    int total_subsongs;
} ea_header;

static int parse_header(STREAMFILE* streamFile, ea_header* ea, off_t begin_offset);
static VGMSTREAM * init_vgmstream_main(STREAMFILE *streamFile, ea_header* ea);

static void set_ea_1snh_num_samples(STREAMFILE* streamFile, off_t start_offset, ea_header* ea);
static int get_ea_1snh_ima_version(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea);

/* EA 1SNh - from early EA games, stream (~1996, ex. Need for Speed) */
VGMSTREAM * init_vgmstream_ea_1snh(STREAMFILE *streamFile) {
    ea_header ea = {0};
    off_t eacs_offset;


    /* checks */
    /* .asf/as4: common,
     * .lasf: fake for plugins
     * .cnk: some PS games
     * .sng: fake for plugins (to mimic EA SCHl's common extension)
     * .uv/tgq: some SAT games (video only?) */
    if (!check_extensions(streamFile,"asf,lasf,as4,cnk,sng,uv,tgq"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x31534E68 &&  /* "1SNh" */
        read_32bitBE(0x00,streamFile) != 0x53454144)    /* "SEAD" */
        goto fail;

    /* stream is divided into blocks/chunks: 1SNh=audio header, 1SNd=data xN, 1SNl=loop end, 1SNe=end.
     * Video uses various blocks (kVGT/fVGT/etc) and sometimes alt audio blocks (SEAD/SNDC/SEND). */
    ea.is_sead = read_32bitBE(0x00,streamFile) == 0x53454144;

    /* use block size as endian marker (Saturn = BE) */
    ea.big_endian = guess_endianness32bit(0x04,streamFile);

    eacs_offset = 0x08; /* after 1SNh block id+size */

    if (!parse_header(streamFile,&ea, eacs_offset))
        goto fail;
    return init_vgmstream_main(streamFile, &ea);

fail:
    return NULL;
}

/* EA EACS - from early EA games, bank (~1996, ex. Need for Speed) */
VGMSTREAM * init_vgmstream_ea_eacs(STREAMFILE *streamFile) {
    ea_header ea = {0};
    off_t eacs_offset;


    /* checks */
    /* .eas: single bank [Need for Speed (PC)]
     * .bnk: multi bank [Need for Speed (PC)] */
    if (!check_extensions(streamFile,"eas,bnk"))
        goto fail;

    /* plain data without blocks, can contain N*(EACS header) + N*(data), or N (EACS header + data) */
    ea.is_bank = 1;

    /* use ??? as endian marker (Saturn = BE) */
    //ea.big_endian = guess_endianness32bit(0x04,streamFile);

    if (read_32bitBE(0x00,streamFile) == 0x45414353) { /* "EACS" */
        /* single bank variant */
        eacs_offset = 0x00;
    }
    else if (read_32bitBE(0x00,streamFile) == 0x00) {
        /* multi bank variant */
        int i;
        int target_subsong = streamFile->stream_index;

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0) goto fail;

        /* offsets to EACSs are scattered in the first 0x200
         * this looks dumb but seems like the only way */
        eacs_offset = 0;
        for (i = 0x00; i < 0x200; i += 0x04) {
            off_t bank_offset = read_32bitLE(i, streamFile);
            if (bank_offset == 0)
                continue;

            ea.total_subsongs++;

            /* parse mini bank header */
            if (ea.total_subsongs == target_subsong) {
                /* 0x00: some id or flags? */
                eacs_offset = read_32bitLE(bank_offset + 0x04, streamFile);
                /* rest: not sure if part of this header */
            }
        }

        if (eacs_offset == 0)
            goto fail;
    }
    else {
        goto fail;
    }

    if (!parse_header(streamFile,&ea, eacs_offset))
        goto fail;
    return init_vgmstream_main(streamFile, &ea);

fail:
    return NULL;
}


static VGMSTREAM * init_vgmstream_main(STREAMFILE *streamFile, ea_header* ea) {
    VGMSTREAM * vgmstream = NULL;


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

        case EA_CODEC_IMA: /* Need for Speed II (PC) */
            if (ea->bits && ea->bits != 2) goto fail; /* only in EACS */
            vgmstream->coding_type = coding_DVI_IMA; /* stereo/mono, high nibble first */
            vgmstream->codec_config = ea->codec_config;
            break;

        case EA_CODEC_PSX: /* Need for Speed (PS) */
            vgmstream->coding_type = coding_PSX;
            break;

        default:
            VGM_LOG("EA EACS: unknown codec 0x%02x\n", ea->codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,ea->data_offset))
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

static int parse_header(STREAMFILE* streamFile, ea_header* ea, off_t offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    if (read_32bitBE(offset+0x00, streamFile) == 0x45414353) { /* "EACS" */
        /* EACS subheader (PC, SAT) */
        ea->sample_rate = read_32bit(offset+0x04, streamFile);
        ea->bits        =  read_8bit(offset+0x08, streamFile);
        ea->channels    =  read_8bit(offset+0x09, streamFile);
        ea->codec       =  read_8bit(offset+0x0a, streamFile);
        ea->type        =  read_8bit(offset+0x0b, streamFile); /* block type? 0=1SNh, -1=bank */
        ea->num_samples = read_32bit(offset+0x0c, streamFile);
        ea->loop_start  = read_32bit(offset+0x10, streamFile);
        ea->loop_end    = read_32bit(offset+0x14, streamFile) + ea->loop_start; /* loop length */
        ea->data_offset = read_32bit(offset+0x18, streamFile); /* 0 when blocked */
        /* 0x1c: pan/volume/etc? (0x7F)
         * rest may be padding/garbage */
        //VGM_ASSERT(ea->type != 0, "EA EACS: unknown type %i\n", ea->type);

        if (ea->codec == EA_CODEC_IMA)
            ea->codec_config = get_ea_1snh_ima_version(streamFile, 0x00, ea);
        /* EACS banks with empty values exist but will be rejected later */
    }
    else if (ea->is_sead) {
        /* alt subheader (found in some PC videos) */
        ea->sample_rate = read_32bit(offset+0x00, streamFile);
        ea->channels    = read_32bit(offset+0x04, streamFile);
        ea->codec       = read_32bit(offset+0x08, streamFile);

        if (ea->codec == EA_CODEC_IMA)
            ea->codec_config = get_ea_1snh_ima_version(streamFile, 0x00, ea);

        set_ea_1snh_num_samples(streamFile, 0x00, ea);
        if (ea->loop_start_offset) /* offset found, now find actual start sample */
            set_ea_1snh_num_samples(streamFile, 0x00, ea);
    }
    else {
        /* alt subheader (PS) */
        ea->sample_rate = read_32bit(offset+0x00, streamFile);
        ea->channels    =  read_8bit(offset+0x18, streamFile);
        ea->codec       = EA_CODEC_PSX;

        set_ea_1snh_num_samples(streamFile, 0x00, ea);
        if (ea->loop_start_offset) /* offset found, now find actual start sample */
            set_ea_1snh_num_samples(streamFile, 0x00, ea);
    }

    ea->loop_flag = (ea->loop_end > 0);

    return 1;
}

/* get total samples by parsing block headers, needed when EACS isn't present */
static void set_ea_1snh_num_samples(STREAMFILE* streamFile, off_t start_offset, ea_header* ea) {
    int num_samples = 0, loop_start = 0, loop_end = 0, loop_start_offset = 0;
    off_t block_offset = start_offset;
    size_t block_size, block_header, block_samples;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;
    size_t file_size = get_streamfile_size(streamFile);


    while (block_offset < file_size) {
        uint32_t id = read_32bitBE(block_offset+0x00,streamFile);
        block_size  =   read_32bit(block_offset+0x04,streamFile);
        block_header = 0;
        block_samples = 0;

        if (id == 0x31534E68 || id == 0x53454144) {  /* "1SNh" "SEAD" audio header */
            int is_sead = (id == 0x53454144);
            int is_eacs = read_32bitBE(block_offset+0x08, streamFile) == 0x45414353;

            block_header = is_eacs ? 0x28 : (is_sead ? 0x14 : 0x2c);
            if (block_header >= block_size) /* sometimes has audio data after header */
                block_header = 0;
        }
        else if (id == 0x31534E64 || id == 0x534E4443) {  /* "1SNd" "SNDC" audio data */
            block_header = 0x08;
        }
        else if (id == 0x00000000 || id == 0xFFFFFFFF || id == 0x31534E65) { /* EOF or "1SNe" */
            break;
        }
        else if (id == 0x31534E6C) {  /* "1SNl" loop point found */
            loop_start_offset = read_32bit(block_offset+0x08,streamFile);
            loop_end = num_samples;
        }

        if (block_header) {
            switch(ea->codec) {
                case EA_CODEC_PSX:
                    block_samples = ps_bytes_to_samples(block_size - block_header, ea->channels);
                    break;
                case EA_CODEC_IMA:
                    if (ea->codec_config == 1)
                        block_samples = read_32bit(block_offset + block_header, streamFile);
                    else
                        block_samples = ima_bytes_to_samples(block_size - block_header, ea->channels);
                    break;
            }
        }


        /* if there is a loop start offset set, this was called again just to find it */
        if (ea->loop_start_offset && ea->loop_start_offset == block_offset) {
            ea->loop_start = num_samples;
            return;
        }

        num_samples += block_samples;
        block_offset += block_size;
    }


    ea->num_samples = num_samples;
    ea->loop_start = loop_start;
    ea->loop_end = loop_end;
    ea->loop_start_offset = loop_start_offset;
}

/* find codec version used, with or without ADPCM hist per block */
static int get_ea_1snh_ima_version(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea) {
    off_t block_offset = start_offset;
    size_t file_size = get_streamfile_size(streamFile);
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    while (block_offset < file_size) {
        uint32_t id = read_32bitBE(block_offset+0x00,streamFile);

        size_t block_size = read_32bitLE(block_offset+0x04,streamFile);
        if (block_size > 0x00F00000) /* BE in SAT, but one file may have both BE and LE chunks */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        if (id == 0x31534E64 || id == 0x534E4443) {  /* "1SNd" "SNDC" audio data */
            size_t ima_samples = read_32bit(block_offset + 0x08, streamFile);
            size_t expected_samples = (block_size - 0x08 - 0x04 - 0x08*ea->channels) * 2 / ea->channels;

            if (ima_samples == expected_samples) {
                return 1; /* has ADPCM hist (hopefully) */
            }
        }

        block_offset += block_size;
    }

    return 0; /* no ADPCM hist */
}
