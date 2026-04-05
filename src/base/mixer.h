#ifndef _MIXER_H_
#define _MIXER_H_

#include "../streamtypes.h"
#include "sbuf.h"

typedef struct mixer_t mixer_t;

/* internal mixing pre-setup for vgmstream (doesn't imply usage).
 * If init somehow fails next calls are ignored. */
mixer_t* mixer_init(int channels);
void mixer_free(mixer_t* mixer);
void mixer_reset(mixer_t* mixer);
void mixer_update_channel(mixer_t* mixer);
void mixer_chain(mixer_t* mixer, sbuf_t* sbuf, int32_t current_pos);
void mixer_resample(mixer_t* mixer, sbuf_t* sbuf);
bool mixer_is_chain_active(mixer_t* mixer);
bool mixer_is_resample_active(mixer_t* mixer);

#endif
