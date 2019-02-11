#include "meta.h"


/* .sfl - odd RIFF-formatted files that go along with .ogg [Hanachirasu (PC), Touhou 10.5 (PC)] */
VGMSTREAM * init_vgmstream_sfl_ogg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;
    int loop_flag = 0;
    int loop_start = 0, loop_end = 0;


    /* checks */
    if (!check_extensions(streamFile, "sfl"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x5346504C) /* "SFPL" */
        goto fail;

    {
        /* try with file.ogg.sfl=header and file.ogg=data [Hanachirasu (PC)] */
        char basename[PATH_LIMIT];
        get_streamfile_basename(streamFile,basename,PATH_LIMIT);
        streamData = open_streamfile_by_filename(streamFile, basename);
        if (!streamData) {
            /* try again with file.sfl=header + file.ogg=data */
            streamData = open_streamfile_by_ext(streamFile,"ogg");
            if (!streamData) goto fail;
        }
        else {
            if (!check_extensions(streamData, "ogg"))
                goto fail;
        }
    }

#ifdef VGM_USE_VORBIS
    /* let the real initer do the parsing */
    vgmstream = init_vgmstream_ogg_vorbis(streamData);
    if (!vgmstream) goto fail;
    vgmstream->meta_type = meta_OGG_SFL;
#else
    goto fail;
#endif

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0x0c;
        size_t riff_size = read_32bitLE(0x04,streamFile);
        size_t file_size = get_streamfile_size(streamFile);

        if (file_size < riff_size+0x08)
            goto fail;

        /* sfl loops come in two varieties:
         * - "cue " (start) + "LIST" with "rgn" (length) [Touhou]
         * - "cue " (start+end) [Hanachirasu]
         * Both may have "LIST" with optional "labl" markers (start+end or start+length in seconds),
         * that can be parsed to get loops, but aren't sample-accurate nor always exist.
         */
        while (current_chunk < file_size) {
            uint32_t chunk_type = read_32bitBE(current_chunk+0x00,streamFile);
            size_t chunk_size   = read_32bitLE(current_chunk+0x04,streamFile);

            if (current_chunk + 0x08 + chunk_size > file_size)
                goto fail;

            switch(chunk_type) {
                case 0x63756520: /* "cue " */
                    switch (read_32bitLE(current_chunk+0x08+0x00, streamFile)) { /* cue count */
                        case 1:
                            loop_start = read_32bitLE(current_chunk+0x08+0x04+0x04, streamFile);
                            break;
                        case 2:
                            loop_start = read_32bitLE(current_chunk+0x08+0x04+0x04, streamFile);
                            loop_end   = read_32bitLE(current_chunk+0x08+0x1c+0x04, streamFile);
                            /* cues can be unordered */
                            if (loop_start > loop_end) {
                                long temp = loop_start;
                                loop_start = loop_end;
                                loop_end = temp;
                            }
                            break;

                        default:
                            goto fail;
                    }
                break;

                case 0x4C495354: /* "LIST" */
                    /* "LIST" is chunk-based too but in practice sfl always follows the same order */
                    switch (read_32bitBE(current_chunk+0x08+0x00, streamFile)) {
                        case 0x6164746C: /* "adtl" */
                            if (read_32bitBE(current_chunk+0x08+0x04, streamFile) == 0x6C747874 &&  /* "ltxt" */
                                read_32bitBE(current_chunk+0x08+0x14, streamFile) == 0x72676E20) {  /* "rgn " */
                                loop_end = read_32bitLE(current_chunk+0x08+0x10, streamFile) + loop_start;
                            }
                            break;

                        case 0x6c61626c: /* "labl" */
                        default:
                            break;
                    }
                    break;

                case 0x53465049: /* "SFPI": filename info */
                default:
                    break;
            }
            current_chunk += 0x08+chunk_size;

            /* there may be padding bytes, included in riff_size but not enough to make a new chunk */
            if (current_chunk + 0x08 > file_size)
                break;
        }
    }

    loop_flag = (loop_end > 0);

    /* install loops (sfl .ogg often has song endings too, use the base .ogg for those) */
    if (loop_flag) {
        vgmstream_force_loop(vgmstream,loop_flag,loop_start, loop_end);
    }

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}
