#ifndef _WWISE_VORBIS_UTILS_H_
#define _WWISE_VORBIS_UTILS_H_

#include "coding.h"

int wwise_vorbis_get_header(STREAMFILE *streamFile, off_t offset, wwise_header_type header_type, int * granulepos, size_t * packet_size, int big_endian);
int wwise_vorbis_rebuild_packet(uint8_t * obuf, size_t obufsize, STREAMFILE *streamFile, off_t offset, vorbis_codec_data * data, int big_endian);
int wwise_vorbis_rebuild_setup(uint8_t * obuf, size_t obufsize, STREAMFILE *streamFile, off_t offset, vorbis_codec_data * data, int big_endian, int channels);


#endif/*_WWISE_VORBIS_UTILS_H_ */
