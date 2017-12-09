#ifndef _VORBIS_CUSTOM_DECODER_H_
#define _VORBIS_CUSTOM_DECODER_H_

#include "../vgmstream.h"
#include "../coding/coding.h"

/* used by vorbis_custom_decoder.c, but scattered in other .c files */
#ifdef VGM_USE_VORBIS
int vorbis_custom_setup_init_fsb(STREAMFILE *streamFile, off_t start_offset, vorbis_custom_codec_data *data);
int vorbis_custom_setup_init_wwise(STREAMFILE *streamFile, off_t start_offset, vorbis_custom_codec_data *data);
int vorbis_custom_setup_init_ogl(STREAMFILE *streamFile, off_t start_offset, vorbis_custom_codec_data *data);
int vorbis_custom_setup_init_sk(STREAMFILE *streamFile, off_t start_offset, vorbis_custom_codec_data *data);
int vorbis_custom_setup_init_vid1(STREAMFILE *streamFile, off_t start_offset, vorbis_custom_codec_data *data);

int vorbis_custom_parse_packet_fsb(VGMSTREAMCHANNEL *stream, vorbis_custom_codec_data *data);
int vorbis_custom_parse_packet_wwise(VGMSTREAMCHANNEL *stream, vorbis_custom_codec_data *data);
int vorbis_custom_parse_packet_ogl(VGMSTREAMCHANNEL *stream, vorbis_custom_codec_data *data);
int vorbis_custom_parse_packet_sk(VGMSTREAMCHANNEL *stream, vorbis_custom_codec_data *data);
int vorbis_custom_parse_packet_vid1(VGMSTREAMCHANNEL *stream, vorbis_custom_codec_data *data);
#endif/* VGM_USE_VORBIS */

#endif/*_VORBIS_CUSTOM_DECODER_H_ */
