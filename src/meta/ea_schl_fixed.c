#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* Possibly the same as EA_CODEC_x in variable SCHl */
#define EA_CODEC_PCM            0x00
#define EA_CODEC_IMA            0x02

typedef struct {
    int8_t version;
    int8_t bps;
    int8_t channels;
    int8_t codec;
    int16_t sample_rate;
    int32_t num_samples;

    int big_endian;
    int loop_flag;
} ea_fixed_header;

static int parse_fixed_header(STREAMFILE* streamFile, ea_fixed_header* ea, off_t begin_offset);


/* EA SCHl with fixed header - from EA games (~1997? ex. NHL 97 PC) */
VGMSTREAM * init_vgmstream_ea_schl_fixed(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t header_size;
    ea_fixed_header ea = {0};


    /* checks */
    /* .asf: original
     * .lasf: fake for plugins */
    if (!check_extensions(streamFile,"asf,lasf"))
        goto fail;

    /* check header (see ea_schl.c for more info about blocks) */
    if (read_32bitBE(0x00,streamFile) != 0x5343486C) /* "SCHl" */
        goto fail;

    header_size = read_32bitLE(0x04,streamFile);

    if (!parse_fixed_header(streamFile,&ea, 0x08))
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

        default:
            VGM_LOG("EA: unknown codec 0x%02x\n", ea.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_fixed_header(STREAMFILE* streamFile, ea_fixed_header* ea, off_t begin_offset) {
    off_t offset = begin_offset;

    if (read_32bitBE(offset+0x00, streamFile) != 0x5041546C &&      /* "PATl" */
        read_32bitBE(offset+0x38, streamFile) != 0x544D706C)        /* "TMpl" */
        goto fail;

    offset += 0x3c; /* after TMpl */
    ea->version = read_8bit(offset+0x00, streamFile);
    ea->bps = read_8bit(offset+0x01, streamFile);
    ea->channels = read_8bit(offset+0x02, streamFile);
    ea->codec = read_8bit(offset+0x03, streamFile);
    VGM_ASSERT(read_16bitLE(offset+0x04, streamFile) != 0, "EA SCHl fixed: unknown1 found\n");
    /* 0x04(16): unknown */
    ea->sample_rate = (uint16_t)read_16bitLE(offset+0x06, streamFile);
    ea->num_samples = read_32bitLE(offset+0x08, streamFile);
    VGM_ASSERT(read_32bitLE(offset+0x0c, streamFile) != -1, "EA SCHl fixed: unknown2 found\n"); /* loop start? */
    VGM_ASSERT(read_32bitLE(offset+0x10, streamFile) != -1, "EA SCHl fixed: unknown3 found\n"); /* loop end? */
    VGM_ASSERT(read_32bitLE(offset+0x14, streamFile) !=  0, "EA SCHl fixed: unknown4 found\n"); /* data start? */
    VGM_ASSERT(read_32bitLE(offset+0x18, streamFile) != -1, "EA SCHl fixed: unknown5 found\n");
    VGM_ASSERT(read_32bitLE(offset+0x1c, streamFile) != 0x7F, "EA SCHl fixed: unknown6 found\n");

    //ea->loop_flag = (ea->loop_end_sample);

    return 1;

fail:
    return 0;
}
