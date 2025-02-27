#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

// if enabled vgmstream weights ~600kb more but doesn't need external setup packets
#ifndef VGM_DISABLE_CODEBOOKS
#include "libs/vorbis_codebooks_fsb.h"
#endif


/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

static int build_header_setup(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf);


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
    vorbis_custom_config* cfg = &data->config;

    // load FSB default blocksizes
    cfg->blocksize_0_exp = vorbis_get_blocksize_exp(2048); //long
    cfg->blocksize_1_exp = vorbis_get_blocksize_exp(256); //short

    data->op.bytes = build_header_identification(data->buffer, data->buffer_size, cfg);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) /* identification packet */
        goto fail;

    data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) /* comment packet */
        goto fail;

    data->op.bytes = build_header_setup(data->buffer, data->buffer_size, cfg->setup_id, sf);
    if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) /* setup packet */
        goto fail; 

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

#ifdef VGM_DISABLE_CODEBOOKS
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

static int build_header_setup(uint8_t* buf, size_t bufsize, uint32_t setup_id, STREAMFILE* sf) {
    int bytes;

#ifndef VGM_DISABLE_CODEBOOKS
    // locate from precompiled list
    bytes = vcb_load_codebook_array(buf, bufsize, setup_id, vcb_list, vcb_list_count);
    if (bytes)
        return bytes;
#else
    // load from external files
    bytes = load_fvs_file(buf, bufsize, setup_id, sf);
    if (bytes)
        return bytes;
#endif

    VGM_LOG("FSB Vorbis: setup_id %08x not found\n", setup_id);
    return 0;
}

#endif
