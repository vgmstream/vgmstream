#ifndef _ONGAKUKAN_ADP_LIB_H_
#define _ONGAKUKAN_ADP_LIB_H_

/* Ongakukan ADPCM codec, found in PS2 and PSP games. */

#include "../../util/reader_sf.h"

/* typedef struct */
typedef struct ongakukan_adp_t ongakukan_adp_t;

/* function declaration for we need to set up the codec data. */
ongakukan_adp_t* ongakukan_adpcm_init(STREAMFILE* sf, long int data_offset, long int data_size,
    bool sound_is_adpcm);

/* function declaration for freeing all memory related to ongakukan_adp_t struct var. */
void ongakukan_adpcm_free(ongakukan_adp_t* handle);
void ongakukan_adpcm_reset(ongakukan_adp_t* handle);
void ongakukan_adpcm_seek(ongakukan_adp_t* handle, long int target_sample);

/* function declaration for when we need to get (and send) certain values from ongakukan_adp_t handle */
long int ongakukan_adpcm_get_num_samples(ongakukan_adp_t* handle);
short* ongakukan_adpcm_get_sample_hist(ongakukan_adp_t* handle);

/* function declaration for actually decoding samples, can't be that hard, right? */
void ongakukan_adpcm_decode_data(ongakukan_adp_t* handle);

#endif
