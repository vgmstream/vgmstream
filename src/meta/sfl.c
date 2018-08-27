#include "meta.h"

static void parse_adtl(off_t adtl_offset, off_t adtl_length, STREAMFILE  *streamFile, long *loop_start, long *loop_end, int *loop_flag);


/* .sfl - odd RIFF-formatted files that go along with .ogg [Hanachirasu (PC), Touhou 10.5 (PC)] */
VGMSTREAM * init_vgmstream_sfl_ogg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;
    int loop_flag = 0;
    long loop_start_ms = -1;
    long loop_end_ms = -1;


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
            /* try again with file.sfl=header + file.ogg=daba */
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
        size_t riff_size, file_size;
        off_t current_chunk = 0x0c; /* start with first chunk */

        riff_size = read_32bitLE(0x04,streamFile);
        file_size = get_streamfile_size(streamFile);
        if (file_size < riff_size+0x08)
            goto fail;

        while (current_chunk < file_size) {
            uint32_t chunk_type = read_32bitBE(current_chunk+0x00,streamFile);
            off_t chunk_size    = read_32bitLE(current_chunk+0x04,streamFile);

            /* There seem to be a few bytes left over, included in the
             * RIFF but not enough to make a new chunk. */
            if (current_chunk+0x08 > file_size) break;

            if (current_chunk+0x08+chunk_size > file_size)
                goto fail;

            switch(chunk_type) {
                case 0x4C495354: /* "LIST" */
                    switch (read_32bitBE(current_chunk+0x08, streamFile)) {
                        case 0x6164746C: /* "adtl" */
                            /* yay, atdl is its own little world */
                            parse_adtl(current_chunk + 0x08, chunk_size, streamFile,
                                    &loop_start_ms,&loop_end_ms,&loop_flag);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            current_chunk += 0x08+chunk_size;
        }
    }

    /* install loops */
    if (loop_flag) {
        int loop_start = (long long)loop_start_ms * vgmstream->sample_rate / 1000;
        int loop_end = (long long)loop_end_ms * vgmstream->sample_rate / 1000;
        vgmstream_force_loop(vgmstream,loop_flag,loop_start, loop_end);
    }
    /* sfl .ogg often has song endings (use the base .ogg for those) */

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}

/* return milliseconds */
static long parse_adtl_marker(unsigned char * marker) {
    long hh,mm,ss,ms;

    if (memcmp("Marker ",marker,7)) return -1;

    if (4 != sscanf((char*)marker+7,"%ld:%ld:%ld.%ld",&hh,&mm,&ss,&ms))
        return -1;

    return ((hh*60+mm)*60+ss)*1000+ms;
}

/* return milliseconds */
static int parse_region(unsigned char * region, long *start, long *end) {
    long start_hh,start_mm,start_ss,start_ms;
    long end_hh,end_mm,end_ss,end_ms;

    if (memcmp("Region ",region,7)) return -1;

    if (8 != sscanf((char*)region+7,"%ld:%ld:%ld.%ld to %ld:%ld:%ld.%ld",
                &start_hh,&start_mm,&start_ss,&start_ms,
                &end_hh,&end_mm,&end_ss,&end_ms))
        return -1;

    *start = ((start_hh*60+start_mm)*60+start_ss)*1000+start_ms;
    *end = ((end_hh*60+end_mm)*60+end_ss)*1000+end_ms;
    return 0;
}

/* loop points have been found hiding here */
static void parse_adtl(off_t adtl_offset, off_t adtl_length, STREAMFILE  *streamFile, long *loop_start, long *loop_end, int *loop_flag) {
    int loop_start_found = 0;
    int loop_end_found = 0;
    off_t current_chunk = adtl_offset+0x04;

    while (current_chunk < adtl_offset + adtl_length) {
        uint32_t chunk_type = read_32bitBE(current_chunk+0x00,streamFile);
        off_t chunk_size    = read_32bitLE(current_chunk+0x04,streamFile);

        if (current_chunk+0x08+chunk_size > adtl_offset+adtl_length)
            return;

        switch(chunk_type) {
            case 0x6c61626c: { /* "labl" */
                unsigned char *labelcontent = malloc(chunk_size-0x04);
                if (!labelcontent) return;
                if (read_streamfile(labelcontent,current_chunk+0x0c, chunk_size-0x04,streamFile) != chunk_size-0x04) {
                    free(labelcontent);
                    return;
                }

                switch (read_32bitLE(current_chunk+8,streamFile)) {
                    case 1:
                        if (!loop_start_found && (*loop_start = parse_adtl_marker(labelcontent)) >= 0)
                            loop_start_found = 1;

                        if (!loop_start_found && !loop_end_found  && parse_region(labelcontent,loop_start,loop_end) >= 0) {
                            loop_start_found = 1;
                            loop_end_found = 1;
                        }

                        break;
                    case 2:
                        if (!loop_end_found && (*loop_end = parse_adtl_marker(labelcontent)) >= 0)
                            loop_end_found = 1;
                        break;
                    default:
                        break;
                }

                free(labelcontent);
                break;
            }
            default:
                break;
        }

        current_chunk += 0x08 + chunk_size;
    }

    if (loop_start_found && loop_end_found)
        *loop_flag = 1;

    /* labels don't seem to be consistently ordered */
    if (*loop_start > *loop_end) {
        long temp = *loop_start;
        *loop_start = *loop_end;
        *loop_end = temp;
    }
}
