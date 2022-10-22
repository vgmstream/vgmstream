#include <string.h>
#include "text_reader.h"
#include "log.h"


/* convenience function to init the above struct */
int text_reader_init(text_reader_t* tr, uint8_t* buf, int buf_size, STREAMFILE* sf, uint32_t offset, uint32_t max_offset) {
    memset(tr, 0, sizeof(text_reader_t));

    if (buf_size <= 1 || !buf || !sf)
        return 0;

    tr->buf = buf;
    tr->buf_size = buf_size;
    tr->sf = sf;
    tr->offset = offset;

    if (!max_offset)
        max_offset = get_streamfile_size(sf);
    tr->max_offset = max_offset;

    return 1;
}


/* reads more data into buf and adjust values */
static void prepare_buf(text_reader_t* tr) {

    /* since we may read N lines in the same buffer, move starting pos each call */
    tr->pos = tr->next_pos;

    /* not more data (but may still read lines so not an error) */
    if (tr->offset >= tr->max_offset) {
        return;
    }

    /* request more data */
    if (tr->pos >= tr->filled) {
        tr->pos = 0;
        tr->filled = 0;
    }

    /* partially filled, move buffer */
    if (tr->pos > 0) {
        int move_size = tr->filled - tr->pos;

        memmove(tr->buf, &tr->buf[tr->pos], move_size); /* memmove = may overlap */
        tr->filled -= tr->pos; /* now less filled */
        tr->pos = 0;
    }

    /* has enough data */
    if (tr->filled >= tr->buf_size) {
        return;
    }

    /* read buf up to max */
    {
        int bytes;
        int read_size = tr->buf_size - tr->filled;
        if (read_size + tr->offset > tr->max_offset)
            read_size = tr->max_offset - tr->offset;

        if (read_size <= 0) { /* ??? */
            bytes = 0;
        }
        else {
            if (tr->filled + read_size >= tr->buf_size)
                read_size -= 1; /* always leave an extra byte for c-string null */

            bytes = read_streamfile(tr->buf + tr->filled, tr->offset, read_size, tr->sf);
            tr->offset += bytes;
            tr->filled += bytes;
        }

        /* maybe some internal issue, force EOF */
        if (bytes == 0) {
            tr->offset = tr->max_offset;
        }

        /* ensure no old data is used as valid (simplifies some checks during parse) */
        tr->buf[tr->filled] = '\0';
    }
}

static void parse_buf(text_reader_t* tr) {
    int i;

    tr->line = (char*)&tr->buf[tr->pos];
    tr->line_len = 0;
    tr->line_ok = 0;

    /* detect EOF (this should only happen if no more data was loaded) */
    if (tr->pos == tr->filled) {
        tr->line = NULL;
        tr->line_ok = 1;
        tr->line_len = 0;
        return;
    }

    /* assumes filled doesn't reach buf_size (to allow trailing \0 after filled) */
    for (i = tr->pos; i < tr->filled; i++) {
        char c = (char)tr->buf[i];

        if (c == '\0') {
            i++;
            break; /* not a valid file? (line_ok=0) */
        }

        if (c == '\r' && tr->buf[i+1] == '\n') { /* CRLF (0x0d0a) */
            /* i+1 may read past filled but it's pre-set to \0 */
            i += 2; //todo check that i < buf_size-1
            tr->line_ok = 1;
            break;
        }
        else if (c == '\n') { /* LF (0x0a) */
            i++;
            tr->line_ok = 1;
            break;
        }
        else if (c == '\r') { /* CR (0x0d) */
            i++;
            tr->line_ok = (i < tr->buf_size - 1);
            /* if buf ends with a CR, next buf may start be a LF (single CRLF), so line is not ok near buf end
             * (old Macs use single \r as lines, but using only that and reaching buf end should happen rarely) */
            break;
        }

        tr->line_len++;
    }

    /* when lines are small may read up to filled smaller than buf, with no more data */
    if (!tr->line_ok && i == tr->filled)
        tr->line_ok = (tr->filled < tr->buf_size - 1);

    /* added after proper line (a \n) or after buf end, so we aren't changing valid data */
    tr->buf[tr->pos + tr->line_len] = '\0';
    tr->next_pos = i;
}

int text_reader_get_line(text_reader_t* tr, char** p_line) {

    if (!tr->buf) /* no init */
        return 0;

    /* how it works:
     * - fills buffer up to max or buf_len, from pos 0
     * - counts from 0 to next '\n' or EOF
     *   - nulls \n or after EOF to make a proper c-string
     * - returns from string from pos 0 to len
     * - on next call rather than re-reading continues from pos N (after \n)
     *   - a buf will likely contain multiple lines
     * - if read chars reach buf_end (no proper line found):
     *   - pos = 0: buf isn't big enough, error
     *   - pos > 0: move data to pos=0, fill rest of buf, fill rest of buf
     *
     * ex. 
     * - parse buf: read chunk full [aaaaa\nbbbb] (pos = 0)
     * - get line: returns "aaaaa\0" (next_pos points to first 'b')
     * - get line: from 'b', but reaches buf end before \n or EOF: must readjust
     * - parse buf: move chunk part [bbbb*******] ('b' to beginning, * is garbage)
     * - parse buf: read chunk part [bbbbbb\ncc_] (reaches EOF)
     * - get line: returns "bbbbbb\0" (pos points to first c)
     * - get line: returns "cc\0"
     * - get line: returns NULL (reached EOF, no more bytes)
     * - (there is an implicit \0 reserved in buf)
     *
     * ex.
     * - start: read chunk [aaaaaaaaaaa]
     * - get line: reaches buf end, but didn't reach EOF nor \n: error, can't store line
    */

    prepare_buf(tr); /* may not do anything */
    parse_buf(tr); /* next line */

    /* if we are reading a partial line there may be more data */
    if (!tr->line_ok && tr->pos > 0) {
        prepare_buf(tr);
        parse_buf(tr); /* could continue from prev parse but makes logic more complex for little gain */
    }

    /* always output line even if truncated */
    if (p_line) *p_line = tr->line;
    return !tr->line_ok ?
        -(tr->line_len + 1) : /* -0 also is possible, force -1 */
        tr->line_len;
}
