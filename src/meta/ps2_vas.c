#include "meta.h"
#include "../coding/coding.h"


/* .VAS - from Konami Jikkyou Powerful Pro Yakyuu games */
VGMSTREAM* init_vgmstream_ps2_vas(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "vas"))
        goto fail;
    if (read_u32le(0x00,sf) + 0x800 != get_streamfile_size(sf))
       goto fail;

    loop_flag = (read_u32le(0x10,sf) != 0);
    channels = 2;
    start_offset = 0x800;

    /* header is too simple so test a bit */
    if (!ps_check_format(sf, start_offset, 0x1000))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_VAS;
    vgmstream->sample_rate = read_s32le(0x04,sf);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;

    vgmstream->num_samples = ps_bytes_to_samples(read_u32le(0x00,sf), channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_u32le(0x14,sf), channels);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .VAS in containers */
VGMSTREAM* init_vgmstream_ps2_vas_container(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset = 0;
    size_t subfile_size = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "vas"))
        goto fail;

    if (read_u32be(0x00, sf) == 0xAB8A5A00) { /* fixed value */

        /* just in case */
        if (read_u32le(0x04, sf) * 0x800 + 0x800 != get_streamfile_size(sf))
            goto fail;

        total_subsongs = read_s32le(0x08, sf); /* also at 0x10 */
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        /* check offset table flag, 0x98 has table size */
        if (read_32bitLE(0x94, sf)) {
            off_t header_offset = 0x800 + 0x10*(target_subsong-1);

            /* some values are repeats found in the file sub-header */
            subfile_offset = read_32bitLE(header_offset + 0x00,sf) * 0x800;
            subfile_size   = read_32bitLE(header_offset + 0x08,sf) + 0x800;
        }
        else {
            /* a bunch of files */
            off_t offset = 0x800;
            int i;

            for (i = 0; i < total_subsongs; i++) {
                size_t size = read_32bitLE(offset, sf) + 0x800;

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
        size_t file_size = get_streamfile_size(sf);
        off_t offset = 0;

        /* must have multiple .vas */
        if (read_32bitLE(0x00,sf) + 0x800 >= file_size)
           goto fail;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        while (offset < file_size) {
            size_t size = read_32bitLE(offset,sf) + 0x800;

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


    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ps2_vas(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
