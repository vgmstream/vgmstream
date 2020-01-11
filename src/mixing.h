#ifndef _MIXING_H_
#define _MIXING_H_

#include "vgmstream.h"

/* Applies mixing commands to the sample buffer. Mixing must be externally enabled and
 * outbuf must big enough to hold output_channels*samples_to_do */
void mix_vgmstream(sample_t *outbuf, int32_t sample_count, VGMSTREAM* vgmstream);

/* internal mixing pre-setup for vgmstream (doesn't imply usage).
 * If init somehow fails next calls are ignored. */
void mixing_init(VGMSTREAM* vgmstream);
void mixing_close(VGMSTREAM* vgmstream);
void mixing_update_channel(VGMSTREAM* vgmstream);

/* Call to let vgmstream apply mixing, which must handle input/output_channels.
 * Once mixing is active any new mixes are ignored (to avoid the possibility
 * of down/upmixing without querying input/output_channels). */
void mixing_setup(VGMSTREAM * vgmstream, int32_t max_sample_count);

/* gets current mixing info */
void mixing_info(VGMSTREAM * vgmstream, int *input_channels, int *output_channels);

/* adds mixes filtering and optimizing if needed */
void mixing_push_swap(VGMSTREAM* vgmstream, int ch_dst, int ch_src);
void mixing_push_add(VGMSTREAM* vgmstream, int ch_dst, int ch_src, double volume);
void mixing_push_volume(VGMSTREAM* vgmstream, int ch_dst, double volume);
void mixing_push_limit(VGMSTREAM* vgmstream, int ch_dst, double volume);
void mixing_push_upmix(VGMSTREAM* vgmstream, int ch_dst);
void mixing_push_downmix(VGMSTREAM* vgmstream, int ch_dst);
void mixing_push_killmix(VGMSTREAM* vgmstream, int ch_dst);
void mixing_push_fade(VGMSTREAM* vgmstream, int ch_dst, double vol_start, double vol_end, char shape, int32_t time_pre, int32_t time_start, int32_t time_end, int32_t time_post);

void mixing_macro_volume(VGMSTREAM* vgmstream, double volume, uint32_t mask);
void mixing_macro_track(VGMSTREAM* vgmstream, uint32_t mask);
void mixing_macro_layer(VGMSTREAM* vgmstream, int max, uint32_t mask, char mode);
void mixing_macro_crosstrack(VGMSTREAM* vgmstream, int max);
void mixing_macro_crosslayer(VGMSTREAM* vgmstream, int max, char mode);
void mixing_macro_downmix(VGMSTREAM* vgmstream, int max /*, mapping_t output_mapping*/);


#endif /* _MIXING_H_ */
