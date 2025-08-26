#ifndef _SEEK_TABLE_H_
#define _SEEK_TABLE_H_

#include "../vgmstream.h"

typedef struct seek_table_t seek_table_t;

typedef struct {
    int32_t sample;    // sample closest to offset
    uint32_t offset;    // preferably absolute offset but depends on codec
} seek_entry_t;


/* Add new seek entry to vgmstream for current codec.
 * First entry doesn't need to be 0.
 */
bool seek_table_add_entry(VGMSTREAM* v, int32_t sample, uint32_t offset);

/* Loads seek entry closest to sample 
 * Returns num samples to skip after seeking, or -1 if not found (no closest entry/empty table).
 */
int32_t seek_table_get_entry(VGMSTREAM* v, int32_t sample, seek_entry_t* entry);

/* Same but gets previous closest entry (for testing setup samples stuff).
 */
int32_t seek_table_get_entry_prev(VGMSTREAM* v, int32_t target_sample, seek_entry_t* entry);

void seek_table_free(VGMSTREAM* vgmstream);

#endif
