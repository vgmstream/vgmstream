#include "seek_table.h"
#include "../util/log.h"

#define GROWTH_FACTOR 2
#define GROWTH_BASE 1024 //typical files have 1000~4000 frames

struct seek_table_t {
    int count;
    int capacity;
    bool reset_decoder;
    seek_entry_t* entries;
};


static bool init_table(VGMSTREAM* v) {
    if (!v->seek_table) {
        v->seek_table = calloc(1, sizeof(seek_table_t));
        if (!v->seek_table) return false;
    }

    return true;
}

bool seek_table_add_entry(VGMSTREAM* v, int32_t sample, uint32_t offset) {
    //;VGM_LOG("SEEK-TABLE: add entry sample=%i, offset=%x\n", sample, offset);
    if (sample < 0 || offset == 0xFFFFFFFF)
        return false;

    if (!init_table(v))
        return false;

    seek_table_t* table = v->seek_table;

    // grow as needed
    if (table->count >= table->capacity) {
        int new_capacity = (table->capacity == 0) ? GROWTH_BASE : table->capacity * GROWTH_FACTOR;
        seek_entry_t* new_entries = realloc(table->entries, new_capacity * sizeof(seek_entry_t));
        if (!new_entries)
            return false;

        table->entries = new_entries;
        table->capacity = new_capacity;

        //;VGM_LOG("SEEK-TABLE: regrow table to %i (sample=%i, offset=%x)\n", new_capacity, sample, offset);
    }

    table->entries[table->count].sample = sample;
    table->entries[table->count].offset = offset;

    table->count++;

    return true;
}

bool seek_table_add_entry_validate(VGMSTREAM* v, int32_t sample, int32_t max_samples, uint32_t offset, uint32_t max_offset) {

    if (sample > max_samples) {
        VGM_LOG("SEEK-TABLE: bad seek entry, packet=%i vs samples=%i (o=%x)\n", sample, max_samples, offset);
        return false;
    }

    if (offset > max_offset) {
        VGM_LOG("SEEK-TABLE: bad seek entry, offset=%x vs max=%x (s=%i)\n", offset, max_offset, sample);
        return false;
    }

    return seek_table_add_entry(v, sample, offset);
}


static int32_t seek_table_get_entry_internal(VGMSTREAM* v, int32_t target_sample, seek_entry_t* entry, bool prev) {
    //;VGM_LOG("SEEK-TABLE: find entry for sample %i\n", target_sample);
    if (!entry)
        return -1;

    seek_table_t* table = v->seek_table;
    if (!table) {
        //;VGM_LOG("SEEK-TABLE: no entries found\n");
        return -1;
    }

    int best_entry = -1;
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].sample > target_sample)
            break;
        best_entry = i;
    }

    // for setup samples stuff
    if (prev) {
        best_entry--;
    }

    if (best_entry < 0) {
        // not found (may happen when looking for sample 0 since often it's not saved)
        //;VGM_LOG("SEEK-TABLE: no closest entry found\n");
        return -1;
    }

    *entry = table->entries[best_entry]; //memcpy
    //;VGM_LOG("SEEK-TABLE: entry %i found (sample=%i, offset=%x, skip=%i)\n", best_entry, entry->sample, entry->offset, target_sample - entry->sample);
    return target_sample - entry->sample;
}


int32_t seek_table_get_entry_prev(VGMSTREAM* v, int32_t target_sample, seek_entry_t* entry) {
    return seek_table_get_entry_internal(v, target_sample, entry, true);
}

int32_t seek_table_get_entry(VGMSTREAM* v, int32_t target_sample, seek_entry_t* entry) {
    return seek_table_get_entry_internal(v, target_sample, entry, false);
}


void seek_table_set_reset_decoder(VGMSTREAM* v) {
    if (!init_table(v))
        return;

    seek_table_t* table = v->seek_table;
    if (!table)
        return;

    table->reset_decoder = true;
}

bool seek_table_get_reset_decoder(VGMSTREAM* v) {
    seek_table_t* table = v->seek_table;
    if (!table)
        return false;

    return table->reset_decoder;
}


void seek_table_free(VGMSTREAM* v) {
    if (!v)
        return;

    seek_table_t* table = v->seek_table;
    if (!table)
        return;

    free(table->entries);
    free(table);
}
