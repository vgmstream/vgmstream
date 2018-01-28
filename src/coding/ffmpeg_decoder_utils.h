#ifndef _FFMPEG_DECODER_UTILS_
#define _FFMPEG_DECODER_UTILS_

#ifdef VGM_USE_FFMPEG
/* used by ffmpeg_decoder.c, but scattered in other .c files */

/**
 * Custom read/seek for data transformation. Must handle seeks+reads from virtual offsets, ie.-
 * reads "real/file" data, not decodable by FFmpeg, and transforms to decodable "virtual/buffer" data,
 * block by block (must seek to closest file offset and adjust on reads).
 *
 * To simplify, functions won't be called in common cases (seek over filesize, no change in offset, etc),
 * and fake header seeks/reads are handled externally. Real offset must be updated internally though.
 *
 * example (a 0x100 block transforms to a 0x150 block):
 * - seek 0: file-offset=0, virtual-offset=0
 * - read 0x150: file-read=0x100 transforms to buffer=0x150
 * - new file-offset=0x100, virtual-offset=0x150
 * - seek 0x310: file-offset=0x200, virtual-offset=0x310 (closest virtual block is 0x150+0x150, + 0x10 adjusted on reads)
 */

int ffmpeg_custom_read_standard(ffmpeg_codec_data *data, uint8_t *buf, int buf_size);
int64_t ffmpeg_custom_seek_standard(ffmpeg_codec_data *data, int64_t virtual_offset);
int64_t ffmpeg_custom_size_standard(ffmpeg_codec_data *data);

int ffmpeg_custom_read_eaxma(ffmpeg_codec_data *data, uint8_t *buf, int buf_size);
int64_t ffmpeg_custom_seek_eaxma(ffmpeg_codec_data *data, int64_t virtual_offset);
int64_t ffmpeg_custom_size_eaxma(ffmpeg_codec_data *data);

int ffmpeg_custom_read_switch_opus(ffmpeg_codec_data *data, uint8_t *buf, int buf_size);
int64_t ffmpeg_custom_seek_switch_opus(ffmpeg_codec_data *data, int64_t virtual_offset);
int64_t ffmpeg_custom_size_switch_opus(ffmpeg_codec_data *data);

//int ffmpeg_custom_read_ea_schl(ffmpeg_codec_data *data, uint8_t *buf, int buf_size);
//int64_t ffmpeg_custom_seek_ea_schl(ffmpeg_codec_data *data, int64_t virtual_offset);
//int64_t ffmpeg_custom_size_ea_schl(ffmpeg_codec_data *data);

//int ffmpeg_custom_read_sfh(ffmpeg_codec_data *data, uint8_t *buf, int buf_size);
//int64_t ffmpeg_custom_seek_sfh(ffmpeg_codec_data *data, int64_t virtual_offset);
//int64_t ffmpeg_custom_size_sfh(ffmpeg_codec_data *data);

#endif

#endif/*_FFMPEG_DECODER_UTILS_*/
