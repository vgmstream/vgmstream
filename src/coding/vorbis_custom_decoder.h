#ifndef _VORBIS_CUSTOM_DECODER_H_
#define _VORBIS_CUSTOM_DECODER_H_

#include "../vgmstream.h"
#include "../coding/coding.h"

/* used by vorbis_custom_decoder.c, but scattered in other .c files */
#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

/* custom Vorbis without Ogg layer */
struct vorbis_custom_codec_data {
    vorbis_info vi;             /* stream settings */
    vorbis_comment vc;          /* stream comments */
    vorbis_dsp_state vd;        /* decoder global state */
    vorbis_block vb;            /* decoder local state */
    ogg_packet op;              /* fake packet for internal use */

    uint8_t* buffer;            /* internal raw data buffer */
    size_t buffer_size;

    size_t samples_to_discard;  /* for looping purposes */
    int samples_full;           /* flag, samples available in vorbis buffers */

    vorbis_custom_t type;        /* Vorbis subtype */
    vorbis_custom_config config; /* config depending on the mode */

    /* Wwise Vorbis: saved data to reconstruct modified packets */
    uint8_t mode_blockflag[64+1];   /* max 6b+1; flags 'n stuff */
    int mode_bits;                  /* bits to store mode_number */
    uint8_t prev_blockflag;         /* blockflag in the last decoded packet */
    /* Ogg-style Vorbis: packet within a page */
    int current_packet;
    /* reference for page/blocks */
    off_t block_offset;
    size_t block_size;

    int prev_block_samples;     /* count for optimization */
};


int vorbis_custom_setup_init_fsb(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_wwise(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_ogl(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_sk(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_vid1(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_awc(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);

int vorbis_custom_parse_packet_fsb(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_wwise(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_ogl(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_sk(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_vid1(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_awc(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
#endif/* VGM_USE_VORBIS */

#endif/*_VORBIS_CUSTOM_DECODER_H_ */
