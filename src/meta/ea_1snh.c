#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

#define EA_CODEC_PCM            0x00
//#define EA_CODEC_???          0x01 //used in SAT videos
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

    int big_endian;
    int loop_flag;
} ea_header;

static int parse_header(STREAMFILE* streamFile, ea_header* ea, off_t begin_offset);
static void set_ea_1snh_psx_samples(STREAMFILE* streamFile, off_t start_offset, ea_header* ea);

/* EA 1SNh - from early EA games (~1996, ex. Need for Speed) */
VGMSTREAM * init_vgmstream_ea_1snh(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    ea_header ea = {0};


    /* check extension (.asf/as4: common, cnk: some PS games) */
    if (!check_extensions(streamFile,"asf,as4,cnk"))
        goto fail;

    /* check header (first block) */
    if (read_32bitBE(0,streamFile)!=0x31534E68) /* "1SNh" */
        goto fail;

    /* use block size as endian marker (Saturn = BE) */
    ea.big_endian = !(read_32bitLE(0x04,streamFile) < 0x0000FFFF);

    if (!parse_header(streamFile,&ea, 0x08))
        goto fail;

    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea.channels, ea.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ea.sample_rate;
    vgmstream->num_samples = ea.num_samples;
    vgmstream->loop_start_sample = ea.loop_start;
    vgmstream->loop_end_sample = ea.loop_end;

    vgmstream->codec_endian = ea.big_endian;
    vgmstream->layout_type = layout_blocked_ea_1snh;
    vgmstream->meta_type = meta_EA_1SNH;

    switch (ea.codec) {
        case EA_CODEC_PCM:
            vgmstream->coding_type = ea.bits==1 ? coding_PCM8_int : coding_PCM16_int;
            break;

        case EA_CODEC_IMA:
            if (ea.bits!=2) goto fail;
            vgmstream->coding_type = coding_DVI_IMA; /* stereo/mono, high nibble first */
            break;

        case EA_CODEC_PSX:
            vgmstream->coding_type = coding_PSX;
            break;

        default:
            VGM_LOG("EA: unknown codec 0x%02x\n", ea.codec);
            goto fail;
    }

    /* open files; channel offsets are updated below */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    block_update_ea_1snh(start_offset,vgmstream);

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

static int parse_header(STREAMFILE* streamFile, ea_header* ea, off_t offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    if (read_32bitBE(offset+0x00, streamFile) == 0x45414353) { /* "EACS" */
        /* PC/SAT EACS subheader */
        ea->sample_rate = read_32bit(offset+0x04, streamFile);
        ea->bits        =  read_8bit(offset+0x08, streamFile);
        ea->channels    =  read_8bit(offset+0x09, streamFile);
        ea->codec       =  read_8bit(offset+0x0a, streamFile);
        ea->type        =  read_8bit(offset+0x0b, streamFile);
        ea->num_samples = read_32bit(offset+0x0c, streamFile);
        ea->loop_start  = read_32bit(offset+0x10, streamFile);
        ea->loop_end    = read_32bit(offset+0x14, streamFile) + ea->loop_start; /* loop length */
        /* 0x18: data start? (0x00), 0x1c: pan/volume/etc? (0x7F), rest can be padding/garbage */
        VGM_ASSERT(ea->type != 0, "EA EACS: unknown type\n"); /* block type? */
    }
    else {
        /* PS subheader */
        ea->sample_rate = read_32bit(offset+0x00, streamFile);
        ea->channels    =  read_8bit(offset+0x18, streamFile);
        ea->codec       = EA_CODEC_PSX;
        set_ea_1snh_psx_samples(streamFile, 0x00, ea);
        if (ea->loop_start_offset)/* found offset, now find sample start */
            set_ea_1snh_psx_samples(streamFile, 0x00, ea);
    }

    ea->loop_flag = (ea->loop_end > 0);

    return 1;
}

/* get total samples by parsing block headers, needed when EACS isn't present */
static void set_ea_1snh_psx_samples(STREAMFILE* streamFile, off_t start_offset, ea_header* ea) {
    int num_samples = 0, loop_start = 0, loop_end = 0, loop_start_offset = 0;
    off_t block_offset = start_offset;
    size_t file_size = get_streamfile_size(streamFile);
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    while (block_offset < file_size) {
        uint32_t id = read_32bitBE(block_offset+0x00,streamFile);
        size_t block_size = read_32bit(block_offset+0x04,streamFile); /* includes id/size */

        if (id == 0x31534E68) {  /* "1SNh" header block found */
            size_t block_header = read_32bitBE(block_offset+0x08, streamFile) == 0x45414353 ? 0x28 : 0x2c; /* "EACS" */
            if (block_header < block_size) /* sometimes has data */
                num_samples += ps_bytes_to_samples(block_size - block_header, ea->channels);
        }

        if (id == 0x31534E64) {  /* "1SNd" data block found */
            num_samples += ps_bytes_to_samples(block_size - 0x08, ea->channels);
        }

        if (id == 0x31534E6C) {  /* "1SNl" loop point found */
            loop_start_offset = read_32bit(block_offset+0x08,streamFile);
            loop_end = num_samples;
        }

        if (id == 0x00000000 || id == 0xFFFFFFFF) { /* EOF: possible? */
            break;
        }

        /* if there is a loop start offset this was called again just to find it */
        if (ea->loop_start_offset && ea->loop_start_offset == block_offset) {
            ea->loop_start = num_samples;
            return;
        }

        /* any other blocks "1SNl" "1SNe" etc */ //todo parse movie blocks
        block_offset += block_size;
    }


    ea->num_samples = num_samples;
    ea->loop_start = loop_start;
    ea->loop_end = loop_end;
    ea->loop_start_offset = loop_start_offset;
}
