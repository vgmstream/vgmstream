#include "meta.h"
#include "../coding/coding.h"

/* SMC+SMH - from Wangan Midnight 1/R (System246) */
VGMSTREAM * init_vgmstream_smc_smh(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset, header_offset = 0;
    size_t stream_size;
    int loop_flag = 0, channel_count, sample_rate;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "smc"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile, "smh");
    if (!streamHeader) goto fail;


    total_subsongs = read_32bitLE(0x00,streamHeader);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    if (total_subsongs*0x10 + 0x10 != get_streamfile_size(streamHeader))
        goto fail;

    header_offset = 0x10 + (target_subsong-1)*0x10;

    start_offset  = read_32bitLE(header_offset+0x00, streamHeader);
    stream_size   = read_32bitLE(header_offset+0x04, streamHeader);
    sample_rate   = read_32bitLE(header_offset+0x08, streamHeader);
    /* 0x0c(2): always 0x10, frame size? */
    channel_count = read_16bitLE(header_offset+0x0e, streamHeader);
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size,channel_count);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SMC_SMH;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x04, streamHeader);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
