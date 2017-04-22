#include "meta.h"
#include "../util.h"


/* SXD - Sony's SDK format? (cousin of SGXD) [Gravity Rush, Freedom Wars, Soul Sacrifice PSV] */
VGMSTREAM * init_vgmstream_sxd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset, chunk_offset, first_offset = 0x60;

    int is_separate;
    int loop_flag, channels, type;
    int sample_rate, num_samples, loop_start_sample, loop_end_sample;
    int target_stream = 0, total_streams;


    /* check extension, case insensitive */
    /* .sxd: header+data (SXDF), .sxd1: header (SXDF) + .sxd2 = data (SXDS) */
    if (!check_extensions(streamFile,"sxd,sxd2")) goto fail;
    is_separate = !check_extensions(streamFile,"sxd");

    /* sxd1+sxd2: use sxd1 as header; otherwise use the current file as header */
    if (is_separate) {
        if (read_32bitBE(0x00,streamFile) != 0x53584453) /* "SXDS" */
            goto fail;
        streamHeader = open_stream_ext(streamFile, "sxd1");
        if (!streamHeader) goto fail;
    } else {
        streamHeader = streamFile;
    }
    if (read_32bitBE(0x00,streamHeader) != 0x53584446) /* "SXDF" */
        goto fail;


    /* typical chunks: NAME, WAVE and many control chunks (SXDs don't need to contain any sound data) */
    /* WAVE chunk (0 + streams + 4*streams table + streams * variable? + optional padding) */
    if (!find_chunk_le(streamHeader, 0x57415645,first_offset,0, &chunk_offset,NULL)) goto fail; /* "WAVE" */

    /* check multi-streams (usually only in SFX containers) */
    total_streams = read_32bitLE(chunk_offset+0x04,streamHeader);
    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || target_stream > total_streams || total_streams < 1) goto fail;

    /* read stream header */
    {
        off_t table_offset, header_offset, stream_offset, data_offset;
        int i;

        /* get target offset using table of relative offsets within WAVE */
        table_offset  = chunk_offset + 0x08 + 4*(target_stream-1);
        header_offset = table_offset + read_32bitLE(table_offset,streamHeader);

        type        = read_32bitLE(header_offset+0x00,streamHeader);
        /* 0x04 (1): unk (HEVAG: 21, ATRAC9: 42 */
        channels    = read_8bit   (header_offset+0x05,streamHeader);
        sample_rate = read_32bitLE(header_offset+0x08,streamHeader);
        /* 0x0c (4): unk size? */
        /* 0x10 (4): ? + volume? + pan? (can be 0 for music) */
        num_samples = read_32bitLE(header_offset+0x14,streamHeader);
        loop_start_sample = read_32bitLE(header_offset+0x18,streamHeader);
        loop_end_sample   = read_32bitLE(header_offset+0x1c,streamHeader);
        /* 0x20 (4): data size */
        /* 0x24 (-): extra data, variable size and format dependant
            (ATRAC9 can contain truncated part of the data, for preloading I guess) */
        loop_flag = loop_start_sample != -1 && loop_end_sample != -1;

        /* calc stream offset by reading stream sizes */
        stream_offset = 0;
        for (i = 0; i < total_streams; i++) {
            off_t  subtable_offset, subheader_offset;
            if (i == target_stream-1) break;

            subtable_offset  = chunk_offset + 0x08 + 4*(i);
            subheader_offset = subtable_offset + read_32bitLE(subtable_offset,streamHeader);
            stream_offset += read_32bitLE(subheader_offset+0x20,streamHeader); /* data size */
        }

        if (is_separate) {
            data_offset = first_offset;
        } else {
            if (!find_chunk_le(streamHeader, 0x44415441,first_offset,0, &data_offset,NULL)) goto fail; /* "DATA" */
            data_offset += 0x08;
        }

        start_offset = data_offset + stream_offset;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_streams;
    vgmstream->meta_type = meta_SXD;


    switch (type) {
        case 0x01:      /* HEVAG */
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

        case 0x03:      /* ATRAC9 */
            goto fail;

        default:
            VGM_LOG("SXD: unknown codec %i", type);
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (is_separate && streamHeader) close_streamfile(streamHeader);
    return vgmstream;

fail:
    if (is_separate && streamHeader) close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
