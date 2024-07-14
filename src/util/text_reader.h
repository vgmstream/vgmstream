#ifndef _TEXT_READER_H_
#define _TEXT_READER_H_


/* Reader tuned for whole text files, reading chunks to minimize I/O with a single buffer.
 * For short lines read_line may be more appropriate (reads up to line end, while this reads bigger chunks),
 * which also allow \0 (this reader returns an error).
 * NOTE: modifies passed buffer (lines are forced to end with \0 rather than \n).
 * 
 * Usage: set text_reader_t and defaults with text_reader_init, call text_reader_get_line(...) to get lines.
 * buf may be size+1 to allow 2^N chunk reads + trailing \0 (better performance?).
 */

#include "../streamfile.h"

typedef struct {
    /* init */
    uint8_t* buf;           /* where data will be read */
    int buf_size;           /* size of the struct (also max line size) */
    STREAMFILE* sf;         /* used to read data */
    uint32_t offset;        /* sf pos */
    uint32_t max_offset;    /* sf max */
  
    /* internal */
    int filled;             /* current buf bytes */
    int pos;                /* current buf pos (last line) */
    int next_pos;           /* buf pos on next call, after line end */
    int line_ok;            /* current line is fully correct */

    char* line;
    int line_len;
} text_reader_t;


/* convenience function to init the above struct */
int text_reader_init(text_reader_t* tr, uint8_t* buf, int buf_size, STREAMFILE* sf, uint32_t offset, uint32_t max);

/* Reads and sets next line, or NULL if no lines are found (EOF).
 * returns line length (0 for empty lines), or <0 if line was too long to store in buf.
 * Will always return a valid (null terminated) string. */
int text_reader_get_line(text_reader_t* tr, char** p_line);

#endif
