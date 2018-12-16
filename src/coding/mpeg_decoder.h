#ifndef _MPEG_DECODER_H_
#define _MPEG_DECODER_H_

#include "../vgmstream.h"
#include "../coding/coding.h"

/* used by mpeg_decoder.c, but scattered in other .c files */
#ifdef VGM_USE_MPEG
typedef struct {
    int version;
    int layer;
    int bit_rate;
    int sample_rate;
    int frame_samples;
    int frame_size; /* bytes */
    int channels;
} mpeg_frame_info;

int mpeg_get_frame_info(STREAMFILE *streamfile, off_t offset, mpeg_frame_info * info);

int mpeg_custom_setup_init_default(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type);
int mpeg_custom_setup_init_ealayer3(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type);
int mpeg_custom_setup_init_awc(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type);
int mpeg_custom_setup_init_eamp3(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type);

int mpeg_custom_parse_frame_default(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream);
int mpeg_custom_parse_frame_ahx(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream);
int mpeg_custom_parse_frame_ealayer3(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream);
int mpeg_custom_parse_frame_awc(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream);
int mpeg_custom_parse_frame_eamp3(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream);

#endif/* VGM_USE_MPEG */

#endif/*_MPEG_DECODER_H_ */
