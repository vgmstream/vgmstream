#include "meta.h"
#include "../coding/coding.h"

/* SSCF - Square-Enix games, older version of .scd [Crisis Core -Final Fantasy VII- (PSP), Dissidia 012 (PSP)] */
VGMSTREAM * init_vgmstream_scd_sscf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, meta_offset, stream_offset;
    size_t stream_size;
    int loop_flag, channel_count, sample_rate;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "scd"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x53534346) /* "SSCF" "*/
        goto fail;
    if (!(read_32bitBE(0x04,streamFile) == 0x002070210 ||    /* version? [Crisis Core (PSP)] */
          read_32bitBE(0x04,streamFile) == 0x10020702))      /* inverted version? [Dissidia (PSP)] */
        goto fail;
    /* 0x08: file size, except for a few files that with a weird value */
    /* 0x10: file id? */


    /* find total subsongs (entries can be dummy) and target subsong */
    {
        int i,j, is_dupe;
        int entries = read_32bitLE(0x0c,streamFile);
        off_t stream_offsets[0x800];

        if (entries > 0x800) /* meh */
            goto fail;


        if (target_subsong == 0) target_subsong = 1;

        meta_offset = 0;
        total_subsongs = 0;
        for (i = 0; i < entries; i++) {
            off_t entry_offset = 0x20 + (0x20*i);
            off_t entry_stream_offset;

            /* skip dummies */
            if (read_32bitLE(entry_offset+0x08,streamFile) == 0) /* size 0 */
                continue;
            if (read_16bitLE(entry_offset+0x0c,streamFile) == 0) /* no sample rate */
                continue;

            /* skip repeated sounds */
            is_dupe = 0;
            entry_stream_offset = read_32bitLE(entry_offset+0x04,streamFile);
            for (j = 0; j < total_subsongs; j++) {
                if (entry_stream_offset == stream_offsets[j]) {
                    is_dupe = 1;
                    break;
                }
            }
            if (is_dupe)
                continue;
            stream_offsets[total_subsongs] = entry_stream_offset;

            /* ok */
            total_subsongs++;
            if (total_subsongs == target_subsong) {
                meta_offset = entry_offset;
            }
        }

        if (meta_offset == 0)
            goto fail;
    }


    /* 0x00(2): config? usually 0x00/0x01 */
    /* 0x02(2): loop config */ //todo when 1 uses PS-ADPCM uses SPU loop flags to set start/end
    stream_offset = read_32bitLE(meta_offset+0x04,streamFile); /* absolute */
    stream_size   = read_32bitLE(meta_offset+0x08,streamFile);
    sample_rate   = (uint16_t)read_16bitLE(meta_offset+0x0c,streamFile);
    /* 0x0e: config? */
    /* 0x10: config? */
    /* 0x14: 0xCA5F or 0x5FCA */
    /* 0x18: config? */
    /* 0x1c: null / some id? */

    loop_flag = 0;
    channel_count = 1;
    start_offset = stream_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_SCD_SSCF;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#if 0
/* SSCF X360 - updated SCD with encrypted data [Final Fantasy XI (360), PlayOnline Viewer (X360)] */
VGMSTREAM * init_vgmstream_scd_sscf_x360(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, meta_offset, stream_offset;
    size_t stream_size;
    int loop_flag, channel_count, sample_rate;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "scd"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x53534346) /* "SSCF" "*/
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x20050300)/* version? */
        goto fail;
    /* 0x08: file size, except for a few files that with a weird value */
    /* 0x0c: null */
    /* 0x10: file id? */
    /* 0x14: encryption key (different files with the same value encrypt the same) */

    /* 0x1c: entry count */

    /* ~0x20: entries start? */
    /* 0x40: num samples? */
    /* 0x44: loop start? */
    /* 0x50: channels */
    /* 0x54: sample rate */

    /* 0x80: encrypted RIFF data */


    loop_flag = 0;
    channel_count = 1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ...;

    vgmstream->meta_type = meta_SCD_SSCF;
    vgmstream->coding_type = coding_...;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
#endif
