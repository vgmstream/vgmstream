#ifndef _ONGAKUKAN_ADP_LIB_H_
#define _ONGAKUKAN_ADP_LIB_H_

/* Ongakukan ADPCM codec, found in PS2 and PSP games. */

#include "../../util/reader_sf.h"

/* typedef struct */
typedef struct ongakukan_adp_t ongakukan_adp_t;

/* function declaration for we need to set up the codec data. */
ongakukan_adp_t* init_ongakukan_adpcm(STREAMFILE* sf, long int data_offset, long int data_size,
    bool sound_is_adpcm);

/* function declaration for freeing all memory related to ongakukan_adp_t struct var. */
void ongakukan_adpcm_free(ongakukan_adp_t* handle);
void ongakukan_adpcm_reset(ongakukan_adp_t* handle);
void ongakukan_adpcm_seek(ongakukan_adp_t* handle, long int target_sample);

/* function declaration for when we need to get (and send) certain values from ongakukan_adp_t handle */
long int get_num_samples_from_ongakukan_adpcm(ongakukan_adp_t* handle);
void* get_sample_hist_from_ongakukan_adpcm(ongakukan_adp_t* handle);

/* function declaration for actually decoding samples, can't be that hard, right? */
void decode_ongakukan_adpcm_data(ongakukan_adp_t* handle);

#endif // _ONGAKUKAN_ADP_LIB_H_
