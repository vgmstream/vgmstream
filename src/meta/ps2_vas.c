#include "meta.h"
#include "../coding/coding.h"


/* .VAS - from Konami Jikkyou Powerful Pro Yakyuu games */
VGMSTREAM * init_vgmstream_ps2_vas(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "vas"))
        goto fail;
    if (read_32bitLE(0x00,streamFile) + 0x800 != get_streamfile_size(streamFile))
       goto fail;

    loop_flag = (read_32bitLE(0x10,streamFile) != 0);
    channel_count = 2;
    start_offset = 0x800;

    /* header is too simple so test a bit */
    if (!ps_check_format(streamFile, start_offset, 0x1000))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_VAS;
    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;

    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x00,streamFile), channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x14,streamFile), channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .VAS in containers */
VGMSTREAM * init_vgmstream_ps2_vas_container(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset = 0;
    size_t subfile_size = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "vas"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) == 0xAB8A5A00) { /* fixed value */

        /* just in case */
        if (read_32bitLE(0x04, streamFile)*0x800 + 0x800 != get_streamfile_size(streamFile))
            goto fail;

        total_subsongs = read_32bitLE(0x08, streamFile); /* also at 0x10 */
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        /* check offset table flag, 0x98 has table size */
        if (read_32bitLE(0x94, streamFile)) {
            off_t header_offset = 0x800 + 0x10*(target_subsong-1);

            /* some values are repeats found in the file sub-header */
            subfile_offset = read_32bitLE(header_offset + 0x00,streamFile) * 0x800;
            subfile_size   = read_32bitLE(header_offset + 0x08,streamFile) + 0x800;
        }
        else {
            /* a bunch of files */
            off_t offset = 0x800;
            int i;

            for (i = 0; i < total_subsongs; i++) {
                size_t size = read_32bitLE(offset, streamFile) + 0x800;

                if (i + 1 == target_subsong) {
                    subfile_offset = offset;
                    subfile_size = size;
                    break;
                }

                offset += size;
            }
            if (i == total_subsongs)
                goto fail;
        }
    }
    else {
        /* some .vas are just files pasted together, better extracted externally but whatevs */
        size_t file_size = get_streamfile_size(streamFile);
        off_t offset = 0;

        /* must have multiple .vas */
        if (read_32bitLE(0x00,streamFile) + 0x800 >= file_size)
           goto fail;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        while (offset < file_size) {
            size_t size = read_32bitLE(offset,streamFile) + 0x800;

            /* some files can be null, ignore */
            if (size > 0x800) {
                total_subsongs++;

                if (total_subsongs == target_subsong) {
                    subfile_offset = offset;
                    subfile_size = size;
                }
            }

            offset += size;
        }

        /* should end exactly at file_size */
        if (offset > file_size)
            goto fail;

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    }


    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, NULL);
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_ps2_vas(temp_streamFile);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
