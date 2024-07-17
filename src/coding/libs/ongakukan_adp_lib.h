#ifndef _ONGAKUKAN_ADP_LIB_
#define _ONGAKUKAN_ADP_LIB_

/* Ongakukan ADP codec, found in PS2 and PSP games. */

#include "../../util/reader_sf.h"

/* typedef struct */
typedef struct ongakukan_adp_t ongakukan_adp_t;

/* function declaration for we need to set up the codec data. */
ongakukan_adp_t* boot_ongakukan_adpcm(STREAMFILE* sf, long int data_offset, long int data_size,
	char sample_needs_setup, char sample_has_base_setup_from_the_start);

/* function declaration for freeing all memory related to ongakukan_adp_t struct var. */
void free_all_ongakukan_adpcm(ongakukan_adp_t* handle);
void reset_all_ongakukan_adpcm(ongakukan_adp_t* handle);
void seek_ongakukan_adpcm_pos(ongakukan_adp_t* handle, long int target_sample);

/* function declaration for when we need to get (and send) certain values from ongakukan_adp_t handle */
long int grab_num_samples_from_ongakukan_adp(ongakukan_adp_t* handle);
long int grab_samples_filled_from_ongakukan_adp(ongakukan_adp_t* handle);
void send_samples_filled_to_ongakukan_adp(long int samples_filled, ongakukan_adp_t* handle);
long int grab_samples_consumed_from_ongakukan_adp(ongakukan_adp_t* handle);
void send_samples_consumed_to_ongakukan_adp(long int samples_consumed, ongakukan_adp_t* handle);
void* grab_sample_hist_from_ongakukan_adp(ongakukan_adp_t* handle);

/* function declaration for actually decoding samples, can't be that hard, right? */
void decode_ongakukan_adp_data(ongakukan_adp_t* handle);

#endif /* _ONGAKUKAN_ADP_LIB_ */
