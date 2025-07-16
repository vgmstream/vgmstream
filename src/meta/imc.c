#include "meta.h"
#include "../coding/coding.h"

/* .IMC (subfile) - from iNiS Gitaroo Man (PS2)  */
VGMSTREAM* init_vgmstream_imc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks */
    /* .imc: extension from the main container */
    if (!check_extensions(sf, "imc"))
        return NULL;

    int channels    = read_s32le(0x00, sf);
    int sample_rate = read_s32le(0x04, sf);
    int interleave  = read_s32le(0x08, sf) * 0x10; // number of frames in a block
    int blocks      = read_s32le(0x0c, sf); // number of interleave blocks (even in mono)

    uint32_t start_offset = 0x10;
    uint32_t file_size = get_streamfile_size(sf);
    bool loop_flag = false;

    // extra checks since the header is so simple
    if (channels < 1 || channels > 8)
        return NULL;

    // Game can play 11025, 16000, 22050, 32000, 44100, 48000. Anything else will be
    //  silent in-game. ST10.IMC subsongs 42-47 use 22000, those are unused silent audio
    if (sample_rate < 11025 || sample_rate > 48000)
        return NULL;

    if (interleave * blocks + start_offset != file_size)
        return NULL;

    uint32_t data_size = file_size - start_offset;
    data_size -= ps_find_padding(sf, start_offset, data_size, channels, interleave, 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IMC;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

/* .IMC in containers */
VGMSTREAM* init_vgmstream_imc_container(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t header_offset, subfile_offset, next_offset, name_offset;
    size_t subfile_size;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "imc"))
        return NULL;

    total_subsongs = read_s32le(0x00, sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    header_offset = 0x04 + 0x20 * (target_subsong - 1);

    name_offset = header_offset + 0x00;
    /* 0x08: flags? (0x702ADE77|0x002ADE77|0x20000000|etc) */
    /* 0x0c: same for all songs in single .imc but varies between .imc */
    subfile_offset = read_u32le(header_offset + 0x10,sf);
    /* 0x14: flags/size? (0xF0950000|0x3CFA1200|etc) */
    /* 0x18: same for all songs in single .imc but varies between .imc */
    /* 0x1c: flags? (0 or 2) */

    if (target_subsong == total_subsongs) {
        next_offset = get_streamfile_size(sf);
    }
    else {
        next_offset = read_u32le(header_offset + 0x20 + 0x10,sf);
    }
    subfile_size = next_offset - subfile_offset;


    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_imc(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    read_string(vgmstream->stream_name,0x08+1, name_offset,sf);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
