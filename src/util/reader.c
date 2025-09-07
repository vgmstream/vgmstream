#include "reader_text.h"
#include "reader_sf.h"
#include "endianness.h"

/* **************************************************** */

size_t read_line(char* buf, int buf_size, off_t offset, STREAMFILE* sf, int* p_line_ok) {
    int i;
    off_t file_size = get_streamfile_size(sf);
    int extra_bytes = 0; /* how many bytes over those put in the buffer were read */

    if (p_line_ok) *p_line_ok = 0;

    for (i = 0; i < buf_size-1 && offset+i < file_size; i++) {
        char in_char = read_8bit(offset+i, sf);
        /* check for end of line */
        if (in_char == 0x0d && read_8bit(offset+i+1, sf) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            if (p_line_ok) *p_line_ok = 1;
            break;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            if (p_line_ok) *p_line_ok = 1;
            break;
        }

        buf[i] = in_char;
    }

    buf[i] = '\0';

    /* did we fill the buffer? */
    if (i == buf_size) {
        char in_char = read_8bit(offset+i, sf);
        /* did the bytes we missed just happen to be the end of the line? */
        if (in_char == 0x0d && read_8bit(offset+i+1, sf) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            if (p_line_ok) *p_line_ok = 1;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            if (p_line_ok) *p_line_ok = 1;
        }
    }

    /* did we hit the file end? */
    if (offset+i == file_size) {
        /* then we did in fact finish reading the last line */
        if (p_line_ok) *p_line_ok = 1;
    }

    return i + extra_bytes;
}

size_t read_bom(STREAMFILE* sf) {
    if (read_u16le(0x00, sf) == 0xFFFE ||
        read_u16le(0x00, sf) == 0xFEFF) {
        return 0x02;
    }

    if ((read_u32be(0x00, sf) & 0xFFFFFF00) == 0xEFBBBF00) {
        return 0x03;
    }

    return 0x00;
}

size_t read_string_sz(char* buf, size_t buf_size, size_t string_size, off_t offset, STREAMFILE* sf) {

    // read up to buf, or stop before if size is set; in either case will stop at 0x00
    size_t max_size = buf_size;
    if (string_size > 0 && string_size < max_size)
        max_size = string_size + 1;

    for (size_t pos = 0; pos < max_size; pos++) {
        uint8_t byte = read_u8(offset + pos, sf);
        if (buf) buf[pos] = (char)byte;

        // done
        if (byte == '\0')
            return pos;

        // null at maxsize and don't validate (expected to be garbage)
        if (pos + 1 == max_size) {
            if (buf) buf[pos] = '\0';
            return max_size;
        }

        // UTF-8 only goes to 0x7F, but allow a bunch of Windows-1252 codes that some games use
        if (byte < 0x20 || byte > 0xF0)
            break;
    }

    // error or wrong max_size
    if (buf) buf[0] = '\0';
    return 0;
}

size_t read_string(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    return read_string_sz(buf, buf_size, 0, offset, sf);
}

size_t read_string_utf16(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf, int big_endian) {
    size_t pos, offpos;
    read_u16_t read_u16 = big_endian ? read_u16be : read_u16le;


    for (pos = 0, offpos = 0; pos < buf_size; pos++, offpos += 2) {
        char c = read_u16(offset + offpos, sf) & 0xFF; /* lower byte for now */
        if (buf) buf[pos] = c;
        if (c == '\0')
            return pos;
        if (pos+1 == buf_size) { /* null at maxsize and don't validate (expected to be garbage) */
            if (buf) buf[pos] = '\0';
            return buf_size;
        }
        if (c < 0x20 || (uint8_t)c > 0xA5)
            goto fail;
    }

fail:
    if (buf) buf[0] = '\0';
    return 0;
}

size_t read_string_utf16le(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    return read_string_utf16(buf, buf_size, offset, sf, 0);
}
size_t read_string_utf16be(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf) {
    return read_string_utf16(buf, buf_size, offset, sf, 1);
}
