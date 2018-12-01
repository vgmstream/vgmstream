#include "meta.h"
#include <ctype.h>


/* .sli+ogg/opus - KiriKiri engine / WaveLoopManager loop points loader [Fate/Stay Night (PC), World End Economica (PC)] */
VGMSTREAM * init_vgmstream_sli_ogg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;
    int32_t loop_start = -1, loop_length = -1;
    int32_t loop_from = -1, loop_to = -1;

    /* checks */
    if (!check_extensions(streamFile, "sli"))
        goto fail;

    {
        /* try with file.ogg/opus.sli=header and file.ogg/opus=data */
        char basename[PATH_LIMIT];
        get_streamfile_basename(streamFile,basename,PATH_LIMIT);
        streamData = open_streamfile_by_filename(streamFile, basename);
        if (!streamData) goto fail;
    }


    /* let the real initer do the parsing */
    if (check_extensions(streamData, "ogg")) { /* Fate/Stay Night (PC) */
#ifdef VGM_USE_VORBIS
        vgmstream = init_vgmstream_ogg_vorbis(streamData);
        if (!vgmstream) goto fail;

        vgmstream->meta_type = meta_OGG_SLI;
#else
    goto fail;
#endif
    }
    else if (check_extensions(streamData, "opus")) { /* Sabbat of the Witch (PC) */
#ifdef VGM_USE_FFMPEG
        vgmstream = init_vgmstream_ffmpeg(streamData);
        if (!vgmstream) goto fail;

        /* FFmpeg's Opus encoder delay is borked but no need to fix:
         * somehow sli+opus use 0 in the OpusHead (to simplify looping?) */

        vgmstream->meta_type = meta_OPUS_SLI;
#else
    goto fail;
#endif
    }
    else {
        goto fail;
    }


    /* find loop text */
    {
        char linebuffer[PATH_LIMIT];
        size_t bytes_read;
        off_t sli_offset;
        int done;

        sli_offset = 0;
        while ((loop_start == -1 || loop_length == -1) && sli_offset < get_streamfile_size(streamFile)) {
            char *endptr, *foundptr;

            bytes_read = get_streamfile_text_line(sizeof(linebuffer),linebuffer,sli_offset,streamFile,&done);
            if (!done) goto fail;

            if (memcmp("LoopStart=",linebuffer,10)==0 && linebuffer[10] != '\0') {
                loop_start = strtol(linebuffer+10,&endptr,10);
                if (*endptr != '\0') {
                    loop_start = -1; /* if it didn't parse cleanly */
                }
            }
            else if (memcmp("LoopLength=",linebuffer,11)==0 && linebuffer[11] != '\0') {
                loop_length = strtol(linebuffer+11,&endptr,10);
                if (*endptr != '\0') {
                    loop_length = -1; /* if it didn't parse cleanly */
                }
            }

            /* a completely different format (2.0?), also with .sli extension and can be handled similarly */
            if ((foundptr = strstr(linebuffer,"To=")) != NULL && isdigit(foundptr[3])) {
                loop_to = strtol(foundptr+3,&endptr,10);
                if (*endptr != ';') {
                    loop_to = -1;
                }
            }
            if ((foundptr = strstr(linebuffer,"From=")) != NULL && isdigit(foundptr[5])) {
                loop_from = strtol(foundptr+5,&endptr,10);
                if (*endptr != ';') {
                    loop_from = -1;
                }
            }

            sli_offset += bytes_read;
        }
    }

    if (loop_start != -1 && loop_length != -1) { /* v1 */
        vgmstream_force_loop(vgmstream,1,loop_start, loop_start+loop_length);
    }
    else if (loop_from != -1 && loop_to != -1) { /* v2 */
        vgmstream_force_loop(vgmstream,1,loop_to, loop_from);
    }
    else {
        goto fail; /* if there's no loop points the .sli wasn't valid */
    }

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}
