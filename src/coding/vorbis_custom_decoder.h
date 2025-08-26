#ifndef _VORBIS_CUSTOM_DECODER_H_
#define _VORBIS_CUSTOM_DECODER_H_

#include "../vgmstream.h"
#include "../coding/coding.h"

/* used by vorbis_custom_decoder.c, but scattered in other .c files */
#ifdef VGM_USE_VORBIS
#include <vorbis/codec.h>

#define MAX_PACKET_SIZES 160 // max 256 in theory, observed max is ~65, rarely ~130

typedef enum { WWV_HEADER_TRIAD, WWV_FULL_SETUP, WWV_INLINE_CODEBOOKS, WWV_EXTERNAL_CODEBOOKS, WWV_AOTUV603_CODEBOOKS } wwise_setup_t;
typedef enum { WWV_TYPE_8, WWV_TYPE_6, WWV_TYPE_2 } wwise_header_t;
typedef enum { WWV_STANDARD, WWV_MODIFIED } wwise_packet_t;

/* custom Vorbis without Ogg layer */
struct vorbis_custom_codec_data {
    vorbis_info vi;             /* stream settings */
    vorbis_comment vc;          /* stream comments */
    vorbis_dsp_state vd;        /* decoder global state */
    vorbis_block vb;            /* decoder local state */
    ogg_packet op;              /* fake packet for internal use */

    uint8_t* buffer;            /* internal raw data buffer */
    size_t buffer_size;
    float* fbuf;

    int current_discard;        /* for looping purposes */

    vorbis_custom_t type;        /* Vorbis subtype */
    vorbis_custom_config config; /* config depending on the mode */

    /* Wwise Vorbis config */
    wwise_setup_t setup_type;
    wwise_header_t header_type;
    wwise_packet_t packet_type;

    /* Wwise Vorbis: saved data to reconstruct modified packets */
    uint8_t mode_blockflag[64+1];   /* max 6b+1; flags 'n stuff */
    int mode_bits;                  /* bits to store mode_number */
    uint8_t prev_blockflag;         /* blockflag in the last decoded packet */

    /* OOR/OggS: current page info (state) */
    int current_packet;
    int packet_count;
    uint16_t packet_size[MAX_PACKET_SIZES];
    uint8_t flags;

    /* reference for page/blocks */
    off_t block_offset;
    size_t block_size;
};


int vorbis_custom_setup_init_fsb(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_wwise(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_ogl(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_sk(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_vid1(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_awc(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);
int vorbis_custom_setup_init_oor(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data);

int vorbis_custom_parse_packet_fsb(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_wwise(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_ogl(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_sk(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_vid1(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_awc(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);
int vorbis_custom_parse_packet_oor(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data);

/* other utils to make/parse vorbis stuff */
int build_header_comment(uint8_t* buf, int bufsize);
int build_header_identification(uint8_t* buf, int bufsize, vorbis_custom_config* cfg);
int vorbis_get_blocksize_exp(int blocksize);
bool load_header_packet(STREAMFILE* sf, vorbis_custom_codec_data* data, uint32_t packet_size, int packet_skip, uint32_t* p_offset);


#endif

#endif
