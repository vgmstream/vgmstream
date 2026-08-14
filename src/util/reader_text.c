#include "reader_text.h"
#include "reader_sf.h"
#include "endianness.h"


size_t read_line(char* buf, int buf_size, off_t offset, STREAMFILE* sf, int* p_line_ok) {
    off_t file_size = get_streamfile_size(sf); //TODO allow supply externally?
    int extra_bytes = 0; // how many bytes over those in the buffer were read

    int line_ok = false;
    if (p_line_ok)
        *p_line_ok = line_ok;

    if (buf <= 0)
        return 0;

    int i;
    for (i = 0; i < buf_size - 1 && offset + i < file_size; i++) {
        char in_char = read_u8(offset+i, sf);
        /* check for end of line */
        if (in_char == 0x0d && read_u8(offset+i+1, sf) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            line_ok = true;
            break;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            line_ok = true;
            break;
        }

        buf[i] = in_char;
    }

    buf[i] = '\0';

    /* did we fill the buffer? */
    if (i == buf_size - 1) {
        char in_char = read_8bit(offset+i, sf);
        /* did the bytes we missed just happen to be the end of the line? */
        if (in_char == 0x0d && read_8bit(offset+i+1, sf) == 0x0a) { /* CRLF */
            extra_bytes = 2;
            line_ok = true;
        }
        else if (in_char == 0x0d || in_char == 0x0a) { /* CR or LF */
            extra_bytes = 1;
            line_ok = true;
        }
    }

    /* did we hit the file end? */
    if (offset + i == file_size) {
        /* then we did in fact finish reading the last line */
        line_ok = true;
    }

    if (p_line_ok)
        *p_line_ok = line_ok;
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


// read up to buf_size, or up to str_size; in either case it will stop
// at \0 and will null-terminate if end is found without null. ex.:
// - "string\0" + buf_size = 8 -> reads 7 = "string\0"
// - "string\0" + buf_size = 5 -> reads 5 = "stri\0" (but last char becomes null)
// - "string\0" + str_size = 4 -> reads 4 = "stri\0" (buf has 5)
// - "string\0" + buf_size = 4 + str_size = 4 -> reads 4 = "str\0" (but last char becomes null)
size_t read_string_sz(char* buf, size_t buf_size, size_t str_size, off_t offset, STREAMFILE* sf) {
    if (buf && buf_size == 0)
        return 0;

    size_t pos;
    for (pos = 0; pos < buf_size; pos++) {
        // str_size has been read + extra null added
        if (str_size && pos == str_size) {
            if (buf) buf[pos] = '\0';
            return pos;
        }

        uint8_t byte = read_u8(offset + pos, sf);
        if (buf) buf[pos] = (char)byte;

        // found null in file
        if (byte == '\0')
            return pos; // + 1;


        // reached allowed max
        if (pos == buf_size - 1) {
            if (buf) buf[pos] = '\0';
            return pos; // + 1;
        }

        // allow a bunch of Windows-1252 codes that some games use
        if (byte < 0x20 || byte > 0xF0)
            break;
    }

    // error or max_size reached
    if (buf) buf[pos] = '\0';
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

/* simple text detection, mainly to reject formats that start with fourcc + size (which includes low bytes)
 * Could be improved but allows high bits for UTF-8 and bytes after \r \n
 */
bool is_text32(uint32_t value) {
    // naive approach, doesn't seem optimized by compilers
    //if ((value & 0xFF000000) < 0x0A000000) return 1;
    //if ((value & 0x00FF0000) < 0x000A0000) return 1;
    //if ((value & 0x0000FF00) < 0x00000A00) return 1;
    //if ((value & 0x000000FF) < 0x0000000A) return 1;

    // remove bytes and check underflow bits, after removing original high bits
    return (((value - 0x0A0A0A0A) & ~value) & 0x80808080) == 0;
}

bool is_text64(uint64_t value) {
    return (((value - 0x0A0A0A0A0A0A0A0AUL) & ~value) & 0x8080808080808080UL) == 0;
}
