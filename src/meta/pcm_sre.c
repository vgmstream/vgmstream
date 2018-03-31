#include "meta.h"
#include "../coding/coding.h"

/* .PCM+SRE. - Capcom's header+data container thing [Viewtiful Joe (PS2)] */
VGMSTREAM * init_vgmstream_pcm_sre(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t table1_entries, table2_entries;
    off_t table1_offset, table2_offset, header_offset;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    /* .pcm=data, .sre=header */
    if (!check_extensions(streamFile, "pcm"))
        goto fail;

    /* first PS-ADPCM frame should be is null */
    if (read_32bitBE(0x00,streamFile) != 0x00020000 ||
        read_32bitBE(0x04,streamFile) != 0x00000000 ||
        read_32bitBE(0x08,streamFile) != 0x00000000 ||
        read_32bitBE(0x0c,streamFile) != 0x00000000)
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile, "sre");
    if (!streamHeader) goto fail;

    table1_entries = read_32bitLE(0x00, streamHeader);
    table1_offset  = read_32bitLE(0x04, streamHeader);
    table2_entries = read_32bitLE(0x08, streamHeader);
    table2_offset  = read_32bitLE(0x0c, streamHeader);
    if (table1_entries*0x60 + table1_offset != table2_offset)
        goto fail; /* just in case */

    total_subsongs = table2_entries;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = table2_offset + (target_subsong-1)*0x20;

    channel_count = read_32bitLE(header_offset+0x00,streamHeader);
    loop_flag     = read_32bitLE(header_offset+0x18,streamHeader);
    start_offset  = read_32bitLE(header_offset+0x08,streamHeader);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_16bitLE(header_offset+0x04,streamHeader);
    vgmstream->meta_type = meta_PCM_SRE;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x1000;

    vgmstream->num_samples       = ps_bytes_to_samples(read_32bitLE(header_offset+0x0c,streamHeader), channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(header_offset+0x10,streamHeader)*channel_count, channel_count);
    vgmstream->loop_end_sample   = ps_bytes_to_samples(read_32bitLE(header_offset+0x14,streamHeader)*channel_count, channel_count);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = read_32bitLE(header_offset+0x0c,streamHeader);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
