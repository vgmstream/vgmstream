#include "meta.h"
#include <ctype.h>
#include <stdbool.h>

static bool get_loops(STREAMFILE* sf, int32_t* p_loop_start, int32_t* p_loop_end);

/* .sli+ogg/opus/wav - KiriKiri engine / WaveLoopManager loop points loader [Fate/Stay Night (PC), World End Economica (PC)] */
VGMSTREAM* init_vgmstream_sli_loops(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_data = NULL;
    int32_t loop_start = 0, loop_end = 0;
    VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf) = NULL;

    /* checks */
    /* V1: fixed to 0x900?
     * V2: varies with text size but around 0xC0 */
    if (get_streamfile_size(sf) > 0x2000)
        return NULL;
    if (!check_extensions(sf, "sli"))
        return NULL;

    {
        /* try with file.ogg/opus/wav.sli=header and file.ogg/opus/wav=data */
        char basename[PATH_LIMIT];
        get_streamfile_basename(sf,basename,PATH_LIMIT);
        sf_data = open_streamfile_by_filename(sf, basename);
        if (!sf_data) goto fail;
    }

    if (is_id32be(0x00, sf_data, "OggS")) {
        if (is_id32be(0x1c, sf_data, "Opus")) { /* Sabbat of the Witch (PC) */
            init_vgmstream = init_vgmstream_ogg_opus;
            /* somehow sli+opus use 0 encoder delay in the OpusHead (to simplify looping?) */
        }
        else { /* Fate/Stay Night (PC) */
            init_vgmstream = init_vgmstream_ogg_vorbis;
        }
    }
    else if (is_id32be(0x00, sf_data, "RIFF")) {
        /* Perfect Cherry Blossom Trial+ (PC) (RIFF created by extractor?) */
        init_vgmstream = init_vgmstream_riff;
    }
    else {
        goto fail;
    }

    vgmstream = init_vgmstream(sf_data);
    if (!vgmstream) goto fail;

    if (!get_loops(sf, &loop_start, &loop_end))
        goto fail;

    vgmstream_force_loop(vgmstream, 1, loop_start, loop_end);

    close_streamfile(sf_data);
    return vgmstream;

fail:
    close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}


static bool get_loops(STREAMFILE* sf, int32_t* p_loop_start, int32_t* p_loop_end) {
    int32_t loop_start = -1, loop_length = -1;
    int32_t loop_from = -1, loop_to = -1;
 
    char line[PATH_LIMIT];
    size_t bytes_read;
    off_t sli_offset;
    int line_ok;

    sli_offset = 0;
    while ((loop_start == -1 || loop_length == -1) && sli_offset < get_streamfile_size(sf)) {
        char *endptr, *foundptr;

        bytes_read = read_line(line, sizeof(line), sli_offset, sf, &line_ok);
        if (!line_ok) goto fail;
        sli_offset += bytes_read;
        /* files may be padded with 0s */

        /* comments in v2.0 [Sabbath of the Witch (PC), KARAKARA (PC)] */
        if (line[0] == '#')
            continue;

        if (memcmp("LoopStart=", line,10) == 0 && line[10] != '\0') {
            loop_start = strtol(line + 10, &endptr, 10);
            if (*endptr != '\0') {
                loop_start = -1; /* if it didn't parse cleanly */
            }
        }
        else if (memcmp("LoopLength=", line, 11) == 0 && line[11] != '\0') {
            loop_length = strtol(line + 11, &endptr, 10);
            if (*endptr != '\0') {
                loop_length = -1; /* if it didn't parse cleanly */
            }
        }

        /* a completely different format ("#2.00"?), can be handled similarly */
        if ((foundptr = strstr(line,"To=")) != NULL && isdigit(foundptr[3])) {
            loop_to = strtol(foundptr + 3, &endptr, 10);
            if (*endptr != ';') {
                loop_to = -1;
            }
        }
        if ((foundptr = strstr(line,"From=")) != NULL && isdigit(foundptr[5])) {
            loop_from = strtol(foundptr + 5, &endptr, 10);
            if (*endptr != ';') {
                loop_from = -1;
            }
        }
    }

    if (loop_start != -1 && loop_length != -1) { /* v1 */
        *p_loop_start = loop_start;
        *p_loop_end = loop_start + loop_length;
    }
    else if (loop_from != -1 && loop_to != -1) { /* v2 */
        *p_loop_start = loop_to;
        *p_loop_end = loop_from;
    }
    else {
        goto fail; /* if there's no loop points the .sli wasn't valid */
    }

    return true;
fail:
    return false;
}
