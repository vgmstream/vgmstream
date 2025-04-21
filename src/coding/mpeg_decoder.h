#ifndef _MPEG_DECODER_H_
#define _MPEG_DECODER_H_

#include "../vgmstream.h"
#include "../coding/coding.h"


/* used by mpeg_decoder.c, but scattered in other .c files */
#ifdef VGM_USE_MPEG

/* represents a single MPEG stream */
typedef struct {
    /* buf per stream as sometimes mpg123 must be fed in passes if data is big enough (ex. EALayer3 multichannel) */
    uint8_t* buffer;
    size_t buffer_size;
    size_t bytes_in_buffer;
    bool buffer_full;
    bool buffer_used;

    void* handle;

    float* sbuf;
    int sbuf_size;      // in bytes for mpg123
    int samples_filled;
    int samples_used;

    int current_size_count; /* data read (if the parser needs to know) */
    int current_size_target; /* max data, until something happens */
    int decode_to_discard;  /* discard from this stream only (for EALayer3 or AWC) */

    int channels_per_frame; /* for rare cases that streams don't share this */
} mpeg_custom_stream;

struct mpeg_codec_data {
    /* regular/single MPEG internals */
    uint8_t* buffer; /* raw data buffer */
    int buffer_size;
    int bytes_in_buffer;
    bool buffer_full; /* raw buffer has been filled */
    bool buffer_used; /* raw buffer has been fed to the decoder */

    void* handle; // MPEG decoder

    /* for internal use */
    int channels_per_frame;
    int samples_per_frame;
    /* for some calcs */
    int bitrate;
    int sample_rate;
    bool is_vbr;

    /* custom MPEG internals */
    bool custom; /* flag */
    mpeg_custom_t type; /* mpeg subtype */
    mpeg_custom_config config; /* config depending on the mode */

    size_t default_buffer_size;
    mpeg_custom_stream* streams; /* array of MPEG streams (ex. 2ch+2ch) */
    int streams_count;

    int skip_samples; /* base encoder delay */
    int samples_to_discard; /* for custom mpeg looping */

    float* sbuf;                // decoded samples from all streams
    int sbuf_size;              // in bytes for mpg123
};

int mpeg_custom_setup_init_default(STREAMFILE* sf, off_t start_offset, mpeg_codec_data* data, coding_t* coding_type);
int mpeg_custom_setup_init_ealayer3(STREAMFILE* sf, off_t start_offset, mpeg_codec_data* data, coding_t* coding_type);
int mpeg_custom_setup_init_awc(STREAMFILE* sf, off_t start_offset, mpeg_codec_data* data, coding_t* coding_type);
int mpeg_custom_setup_init_eamp3(STREAMFILE* sf, off_t start_offset, mpeg_codec_data* data, coding_t* coding_type);

int mpeg_custom_parse_frame_default(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream);
int mpeg_custom_parse_frame_ahx(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream);
int mpeg_custom_parse_frame_ealayer3(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream);
int mpeg_custom_parse_frame_awc(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream);
int mpeg_custom_parse_frame_eamp3(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream);
#endif/* VGM_USE_MPEG */


#endif/*_MPEG_DECODER_H_ */
