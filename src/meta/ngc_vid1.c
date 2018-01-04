#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"


/* VID1 - from Neversoft games (Gun, Tony Hawk's American Wasteland GC) */
VGMSTREAM * init_vgmstream_ngc_vid1(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset, header_size;
    int loop_flag = 0, channel_count;


    /* check extension */
    if (!check_extensions(streamFile,"ogg,logg"))
        goto fail;

    /* chunked/blocked format containing video or audio frames */
    if (read_32bitBE(0x00,streamFile) != 0x56494431) /* "VID1" */
        goto fail;

    /* find actual header start/size in the chunks */
    {
        header_offset = read_32bitBE(0x04, streamFile);
        if (read_32bitBE(header_offset,streamFile) != 0x48454144) /* "HEAD" */
            goto fail;
        start_offset = header_offset + read_32bitBE(header_offset + 0x04, streamFile);
        header_offset += 0x0c;

        /* videos have VIDH before AUDH, and VIDD in blocks, but aren't parsed ATM */

        if (read_32bitBE(header_offset,streamFile) != 0x41554448) /* "AUDH" */
            goto fail;
        header_size = read_32bitBE(header_offset + 0x04, streamFile);
        header_offset += 0x0c;

        if (read_32bitBE(header_offset,streamFile) != 0x56415544) /* "VAUD" (Vorbis audio?) */
            goto fail;
        header_offset += 0x04;
        header_size -= 0x10;

    }
    channel_count   = read_8bit(header_offset + 0x04,streamFile);
    /* other values unknown, maybe related to vorbis (ex. bitrate/encoding modes) */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(header_offset + 0x00, streamFile);
    vgmstream->num_samples = read_32bitBE(header_offset + 0x1c, streamFile);
    vgmstream->meta_type = meta_NGC_VID1;

#ifdef VGM_USE_VORBIS
    {
        vorbis_custom_config cfg = {0};

        vgmstream->layout_type = layout_none;
        vgmstream->coding_type = coding_VORBIS_custom;
        vgmstream->codec_data = init_vorbis_custom(streamFile, header_offset + 0x20, VORBIS_VID1, &cfg);
        if (!vgmstream->codec_data) goto fail;
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
