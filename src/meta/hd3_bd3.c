#include "meta.h"
#include "../coding/coding.h"

 /* HD3+BD3 - Sony PS3 bank format [Elevator Action Deluxe (PS3), R-Type Dimensions (PS3)] */
VGMSTREAM * init_vgmstream_hd3_bd3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset;
    int channel_count, loop_flag, sample_rate;
    size_t interleave, stream_size;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "bd3"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile,"hd3");
    if (!streamHeader) goto fail;

    if (read_32bitBE(0x00,streamHeader) != 0x50334844) /* "P3HD" */
        goto fail;

    /* 0x04: section size (not including first 0x08) */
    /* 0x08: version? 0x00020000 */
    /* 0x10: "P3PG" offset (seems mostly empty and contains number of subsongs towards the end) */
    /* 0x14: "P3TN" offset (some kind of config?) */
    /* 0x18: "P3VA" offset (VAG headers) */
    {
        off_t section_offset = read_32bitBE(0x18,streamHeader);
        off_t header_offset;
        size_t section_size;
        int i, entries, is_bgm = 0;

        if (read_32bitBE(section_offset+0x00,streamHeader) != 0x50335641) /* "P3VA" */
            goto fail;
        section_size = read_32bitBE(section_offset+0x04,streamHeader); /* (not including first 0x08) */
        /* 0x08 size of all subsong headers + 0x10 */

        entries = read_32bitBE(section_offset+0x14,streamHeader);
        /* often there is an extra subsong than written, but may be padding instead */
        if (read_32bitBE(section_offset + 0x20 + entries*0x10 + 0x04,streamHeader)) /* has sample rate */
            entries += 1;

        if (entries * 0x10 > section_size) /* just in case, padding after entries is possible */
            goto fail;

        /* autodetect use of N bank entries as channels [Elevator Action Deluxe (PS3)] */
        if (entries > 1) {
            size_t channel_size = read_32bitBE(section_offset+0x20+0x08,streamHeader);
            is_bgm = 1;

            for (i = 1; i < entries; i++) {
                size_t next_size = read_32bitBE(section_offset+0x20+(i*0x10)+0x08,streamHeader);
                if (channel_size != next_size) {
                    is_bgm = 0;
                    break;
                }
            }
        }

        if (is_bgm) {
            total_subsongs = 1;
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

            header_offset = section_offset+0x20+(0)*0x10;
            start_offset = read_32bitBE(header_offset+0x00,streamHeader); /* 0x00 */
            sample_rate  = read_32bitBE(header_offset+0x04,streamHeader);
            stream_size  = get_streamfile_size(streamFile);
            if (read_32bitBE(header_offset+0x0c,streamHeader) != -1)
                goto fail;

            channel_count = entries;
            interleave = stream_size / channel_count;
        }
        else {
            total_subsongs = entries;
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

            header_offset = section_offset+0x20+(target_subsong-1)*0x10;
            start_offset = read_32bitBE(header_offset+0x00,streamHeader);
            sample_rate  = read_32bitBE(header_offset+0x04,streamHeader);
            stream_size  = read_32bitBE(header_offset+0x08,streamHeader);
            if (read_32bitBE(header_offset+0x0c,streamHeader) != -1)
                goto fail;

            channel_count = 1;
            interleave = 0;
        }
    }

    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channel_count);
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->meta_type = meta_HD3_BD3;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    close_streamfile(streamHeader);
    return vgmstream;
fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
