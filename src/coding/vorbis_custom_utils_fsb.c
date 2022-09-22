#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

#define FSB_VORBIS_USE_PRECOMPILED_FVS 1 /* if enabled vgmstream weights ~600kb more but doesn't need external .fvs packets */
#if FSB_VORBIS_USE_PRECOMPILED_FVS
#include "vorbis_custom_data_fsb.h"
#endif


/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

static int build_header_identification(uint8_t* buf, size_t bufsize, int channels, int sample_rate, int blocksize_short, int blocksize_long);
static int build_header_comment(uint8_t* buf, size_t bufsize);
static int build_header_setup(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf);

#if !(FSB_VORBIS_USE_PRECOMPILED_FVS)
static int load_fvs_file(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf);
#endif
static int load_fvs_array(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * FSB references an external setup packet by the setup_id, and packets have mini headers with the size.
 *
 * Format info from python-fsb5 (https://github.com/HearthSim/python-fsb5) and
 *  fsb-vorbis-extractor (https://github.com/tmiasko/fsb-vorbis-extractor).
 */
int vorbis_custom_setup_init_fsb(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    vorbis_custom_config cfg = data->config;

    data->op.bytes = build_header_identification(data->buffer, data->buffer_size, cfg.channels, cfg.sample_rate, 256, 2048); /* FSB default block sizes */
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */

    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */

    data->op.bytes = build_header_setup(data->buffer, data->buffer_size, cfg.setup_id, sf);
    if (!data->op.bytes) goto fail;
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */

    return 1;

fail:
    return 0;
}


int vorbis_custom_parse_packet_fsb(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    size_t bytes;

    /* get next packet size from the FSB 16b header (doesn't count this 16b) */
    data->op.bytes = read_u16le(stream->offset, stream->streamfile);
    stream->offset += 2;
    if (data->op.bytes == 0 || data->op.bytes == 0xFFFF || data->op.bytes > data->buffer_size) goto fail; /* EOF or end padding */

    /* read raw block */
    bytes = read_streamfile(data->buffer,stream->offset, data->op.bytes,stream->streamfile);
    stream->offset += data->op.bytes;
    if (bytes != data->op.bytes) goto fail; /* wrong packet? */

    return 1;

fail:
    return 0;
}

/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

static int build_header_identification(uint8_t* buf, size_t bufsize, int channels, int sample_rate, int blocksize_short, int blocksize_long) {
    int bytes = 0x1e;
    uint8_t blocksizes, exp_blocksize_0, exp_blocksize_1;

    if (bytes > bufsize) return 0;

    /* guetto log2 for allowed blocksizes (2-exp), could be improved */
    switch(blocksize_long) {
        case 64:   exp_blocksize_0 = 6;  break;
        case 128:  exp_blocksize_0 = 7;  break;
        case 256:  exp_blocksize_0 = 8;  break;
        case 512:  exp_blocksize_0 = 9;  break;
        case 1024: exp_blocksize_0 = 10; break;
        case 2048: exp_blocksize_0 = 11; break;
        case 4096: exp_blocksize_0 = 12; break;
        case 8192: exp_blocksize_0 = 13; break;
        default: return 0;
    }
    switch(blocksize_short) {
        case 64:   exp_blocksize_1 = 6;  break;
        case 128:  exp_blocksize_1 = 7;  break;
        case 256:  exp_blocksize_1 = 8;  break;
        case 512:  exp_blocksize_1 = 9;  break;
        case 1024: exp_blocksize_1 = 10; break;
        case 2048: exp_blocksize_1 = 11; break;
        case 4096: exp_blocksize_1 = 12; break;
        case 8192: exp_blocksize_1 = 13; break;
        default: return 0;
    }
    blocksizes = (exp_blocksize_0 << 4) | (exp_blocksize_1);

    put_u8   (buf+0x00, 0x01);              /* packet_type (id) */
    memcpy     (buf+0x01, "vorbis", 6);     /* id */
    put_u32le(buf+0x07, 0x00);              /* vorbis_version (fixed) */
    put_u8   (buf+0x0b, channels);          /* audio_channels */
    put_s32le(buf+0x0c, sample_rate);       /* audio_sample_rate */
    put_u32le(buf+0x10, 0x00);              /* bitrate_maximum (optional hint) */
    put_u32le(buf+0x14, 0x00);              /* bitrate_nominal (optional hint) */
    put_u32le(buf+0x18, 0x00);              /* bitrate_minimum (optional hint) */
    put_u8   (buf+0x1c, blocksizes);        /* blocksize_0 + blocksize_1 nibbles */
    put_u8   (buf+0x1d, 0x01);              /* framing_flag (fixed) */

    return bytes;
}

static int build_header_comment(uint8_t* buf, size_t bufsize) {
    int bytes = 0x19;

    if (bytes > bufsize) return 0;

    put_u8   (buf+0x00, 0x03);              /* packet_type (comments) */
    memcpy   (buf+0x01, "vorbis", 6);       /* id */
    put_u32le(buf+0x07, 0x09);              /* vendor_length */
    memcpy   (buf+0x0b, "vgmstream", 9);    /* vendor_string */
    put_u32le(buf+0x14, 0x00);              /* user_comment_list_length */
    put_u8   (buf+0x18, 0x01);              /* framing_flag (fixed) */

    return bytes;
}

static int build_header_setup(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf) {
    int bytes;

    /* try to locate from the precompiled list */
    bytes = load_fvs_array(buf, bufsize, setup_id, sf);
    if (bytes)
        return bytes;

#if !(FSB_VORBIS_USE_PRECOMPILED_FVS)
    /* try to load from external files */
    bytes = load_fvs_file(buf, bufsize, setup_id, sf);
    if (bytes)
        return bytes;
#endif

    /* not found */
    VGM_LOG("FSB Vorbis: setup_id %08x not found\n", setup_id);
    return 0;
}

#if !(FSB_VORBIS_USE_PRECOMPILED_FVS)
static int load_fvs_file(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf) {
    STREAMFILE* sf_setup = NULL;

    /* from to get from artificial external file (used if compiled without codebooks) */
    {
        char setupname[0x20];

        snprintf(setupname, sizeof(setupname), ".fvs");
        sf_setup = open_streamfile_by_filename(sf, setupname);
    }

    /* find codebook in mini-header (format by bnnm, feel free to change) */
    if (sf_setup) {
        int entries, i;
        uint32_t offset = 0, size = 0;

        if (!is_id32be(0x00, sf_setup, "VFVS"))
            goto fail;

        entries = read_u32le(0x08, sf_setup); /* 0x04=v0, 0x0c-0x20: reserved */
        if (entries <= 0) goto fail;

        for (i = 0; i < entries; i++) {  /* entry = id, offset, size, reserved */
            if (read_u32le(0x20 + i*0x10, sf_setup) == setup_id) {
                offset = read_u32le(0x24 + i*0x10, sf_setup);
                size = read_u32le(0x28 + i*0x10, sf_setup);
                break;
            }
        }
        if (!size || !offset || size > bufsize) goto fail;

        /* read into buf */
        if (read_streamfile(buf, offset, size, sf_setup) != size)
            goto fail;

        sf_setup->close(sf_setup);
        return size;
    }

fail:
    if (sf_setup) sf_setup->close(sf_setup);
    return 0;
}
#endif

static int load_fvs_array(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf) {
#if FSB_VORBIS_USE_PRECOMPILED_FVS
    int i, list_length;

    list_length = sizeof(fvs_list) / sizeof(fvs_info);
    for (i=0; i < list_length; i++) {
        if (fvs_list[i].id == setup_id) {
            if (fvs_list[i].size > bufsize) goto fail;
            /* found: copy data as-is */
            memcpy(buf,fvs_list[i].setup, fvs_list[i].size);
            return fvs_list[i].size;
        }
    }

fail:
#endif
    return 0;
}

#endif
