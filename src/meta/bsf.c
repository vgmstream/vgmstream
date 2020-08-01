#include "meta.h"
#include "../coding/coding.h"


/* .bsf - from Kuju games [Reign of Fire ((PS2/GC/Xbox)] */
VGMSTREAM* init_vgmstream_bsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_type;
    off_t subfile_name;
    off_t subfile_offset;
    size_t subfile_size;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf,"bsf"))
        goto fail;
    if (read_u32le(0x00,sf) != 0x42534648) /* "BSFH" (notice chunks are LE even on GC) */
        goto fail;

    total_subsongs = read_u32le(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* dumb container of other formats */
    {
        int i;
        off_t offset = 0x08 + read_u32le(0x04,sf); /* 0x04: chunk size */

        subfile_type = 0;
        for (i = 0; i < total_subsongs; i++) {
            /* subsong header "xxxH" */
          //uint32_t head_type = read_u32le(offset + 0x00,sf);
            uint32_t head_size = read_u32le(offset + 0x04,sf);
            /* 0x08: name
             * 0x28: audio config? */
            /* subsong data "xxxD" */
            uint32_t data_type = read_u32le(offset + 0x08 + head_size + 0x00,sf);
            uint32_t data_size = read_u32le(offset + 0x08 + head_size + 0x04,sf);

            if (i + 1 == target_subsong) {
                subfile_name = offset + 0x08;
                subfile_type = data_type;
                subfile_size = data_size;
                subfile_offset = offset + 0x08 + head_size + 0x08;
                break;
            }

            offset += 0x08 + head_size + 0x08 + data_size;
        }

        if (subfile_type == 0)
            goto fail;
    }


    switch(subfile_type) {
        case 0x44535044: /* "DSPD" */
            temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "dsp");
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_ngc_dsp_std(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case 0x56414744: /* "VAGD" */
            temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "vag");
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_vag(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case 0x57415644: /* "WAVD" */
            temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "wav");
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_riff(temp_sf);
            if (!vgmstream) goto fail;
            break;

        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    read_string(vgmstream->stream_name, 0x20, subfile_name, sf);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
