#ifndef _SEEK_TABLE_H_
#define _SEEK_TABLE_H_

#include "../vgmstream.h"

typedef struct seek_table_t seek_table_t;

typedef struct {
    int32_t sample;     // sample closest to offset, preferably including pre-roll
    uint32_t offset;    // offset within file, preferably absolute
} seek_entry_t;


/* Add new seek entry to vgmstream for current codec.
 * Entries must be ordered, and first entry doesn't need to be 0.
 */
bool seek_table_add_entry(VGMSTREAM* v, int32_t sample, uint32_t offset);
bool seek_table_add_entry_validate(VGMSTREAM* v, int32_t sample, int32_t max_samples, uint32_t offset, uint32_t max_offset);

/* Loads seek entry closest to sample 
 * Returns num samples to skip after seeking, or -1 if not found (no closest entry/empty table).
 */
int32_t seek_table_get_entry(VGMSTREAM* v, int32_t sample, seek_entry_t* entry);

/* Same but gets previous closest entry (for testing setup samples stuff).
 */
int32_t seek_table_get_entry_prev(VGMSTREAM* v, int32_t target_sample, seek_entry_t* entry);

/* Mark that seek entries must reset decoder (due to VBR-ness not easily detectable).
 * Otherwise will seek with current state, meaning next samples mix with latest decoded ones.
 *
 * ex.- entry s=1024: can't guess if 1024 will be removed (reset decoder + pre-roll) and equivalent
 * to seeking to 0, or if they won't (pre-roll already done, not safe to reset decoder).
 *
 * Decoder could try to manually remove pre-roll as needed by seeking slightly earlier than requested.
 */
void seek_table_set_reset_decoder(VGMSTREAM* v);
bool seek_table_get_reset_decoder(VGMSTREAM* v);


void seek_table_free(VGMSTREAM* vgmstream);

#endif
