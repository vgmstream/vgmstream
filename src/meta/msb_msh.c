#include "meta.h"
#include "../coding/coding.h"

/* MSB+MSH - SCEE MultiStream flat bank [namCollection: Ace Combat 2 (PS2) sfx, EyeToy Play (PS2)] */
VGMSTREAM * init_vgmstream_msb_msh(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset, header_offset = 0;
    size_t stream_size;
    int loop_flag = 0, channel_count, sample_rate;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "msb"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile, "msh");
    if (!streamHeader) goto fail;

    if (read_32bitLE(0x00,streamHeader) != get_streamfile_size(streamHeader))
        goto fail;
    /* 0x04: unknown */

    /* parse entries */
    {
        int i;
        int entries = read_32bitLE(0x08,streamHeader);

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        for (i = 0; i < entries; i++) {
            if (read_32bitLE(0x0c + 0x10*i, streamHeader) == 0) /* size 0 = empty entry */
                continue;

            total_subsongs++;
            if (total_subsongs == target_subsong && !header_offset) {
                header_offset = 0x0c + 0x10*i;
            }
        }

        if (!header_offset) goto fail;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    }



    loop_flag = 0;
    channel_count = 1;

    stream_size  = read_32bitLE(header_offset+0x00, streamHeader);
    if (read_32bitLE(header_offset+0x04, streamHeader) != 0) /* stereo flag? */
        goto fail;
    start_offset = read_32bitLE(header_offset+0x08, streamHeader);
    sample_rate  = read_32bitLE(header_offset+0x0c, streamHeader); /* Ace Combat 2 seems to set wrong values but probably their bug */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size,channel_count);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_MSB_MSH;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
