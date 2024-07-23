#ifndef _MIXER_H_
#define _MIXER_H_

#include "../streamtypes.h"

/* internal mixing pre-setup for vgmstream (doesn't imply usage).
 * If init somehow fails next calls are ignored. */
void* mixer_init(int channels);
void mixer_free(void* mixer);
void mixer_update_channel(void* mixer);
void mixer_process(void* _mixer, sample_t *outbuf, int32_t sample_count, int32_t current_pos);
bool mixer_is_active(void* mixer);

#endif
