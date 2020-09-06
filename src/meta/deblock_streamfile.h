#ifndef _DEBLOCK_STREAMFILE_H_
#define _DEBLOCK_STREAMFILE_H_
#include "../streamfile.h"

typedef struct deblock_config_t deblock_config_t;
typedef struct deblock_io_data deblock_io_data;

struct deblock_config_t {
    /* config (all optional) */
    size_t logical_size;    /* pre-calculated size for performance (otherwise has to read the whole thing) */
    off_t stream_start;     /* data start */
    size_t stream_size;     /* data max */

    size_t chunk_size;      /* some size like a constant interleave */
    size_t frame_size;      /* some other size */
    size_t skip_size;       /* same */

    int codec;              /* codec or type variations */
    int channels;
    int big_endian;
    uint32_t config;        /* some non-standard config value */

    /* read=blocks from out stream to read) and "steps" (blocks from other streams to skip) */
    int step_start;         /* initial step_count at stream start (often 0) */
    int step_count;         /* number of blocks to step over from other streams */
    //int read_count;         /* number of blocks to read from this stream, after steps */

    size_t track_size;
    int track_number;
    int track_count;
    uint32_t track_type;

    size_t interleave_count;
    size_t interleave_last_count;

    /* callback that setups deblock_io_data state, normally block_size and data_size */
    void (*block_callback)(STREAMFILE* sf, deblock_io_data* data);
    /* callback that alters block, with the current position into the block (0=beginning) */
    void (*read_callback)(uint8_t* dst, deblock_io_data* data, size_t block_pos, size_t read_size);
} ;

struct deblock_io_data {
    /* initial config */
    deblock_config_t cfg;

    /* state */
    off_t logical_offset;   /* fake deblocked offset */
    off_t physical_offset;  /* actual file offset */
    off_t block_size;       /* current block (added to physical offset) */
    off_t skip_size;        /* data to skip from block start to reach data (like a header) */
    off_t data_size;        /* usable data in a block (added to logical offset) */
    off_t chunk_size;       /* current super-block size (for complex blocks, handled manually) */

    int step_count;         /* number of blocks to step over */
    //int read_count;         /* number of blocks to read */

    size_t logical_size;
    size_t physical_size;
    off_t physical_end;
};

STREAMFILE* open_io_deblock_streamfile_f(STREAMFILE* sf, deblock_config_t* cfg);

#endif /* _DEBLOCK_STREAMFILE_H_ */
