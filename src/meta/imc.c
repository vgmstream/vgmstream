#include "meta.h"
#include "../coding/coding.h"

/* .IMC - from iNiS Gitaroo Man (PS2)  */
VGMSTREAM * init_vgmstream_imc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, interleave, blocks;
    size_t file_size, data_size;


    /* checks */
    /* .imc: extension from the main container */
    if (!check_extensions(streamFile, "imc"))
        goto fail;

    channel_count = read_32bitLE(0x00, streamFile);
    sample_rate   = read_32bitLE(0x04, streamFile);
    interleave    = read_32bitLE(0x08, streamFile) * 0x10; /* number of frames in a block */
    blocks        = read_32bitLE(0x0c, streamFile); /* number of interleave blocks (even in mono) */

    file_size = get_streamfile_size(streamFile);
    loop_flag  = 0;
    start_offset = 0x10;

    /* extra checks since the header is so simple */
    if (channel_count < 1 || channel_count > 8)
        goto fail;
    if (sample_rate < 11025 || sample_rate > 48000)
        /* game can play 11025, 16000, 22050, 32000, 44100, 48000. Anything else will be
         silent in-game. ST10.IMC subsongs 42-47 use 22000, those are unused silent audio */
        goto fail;
    if (interleave*blocks + start_offset != file_size)
        goto fail;

    data_size = file_size - start_offset;
    data_size -= ps_find_padding(streamFile, start_offset, data_size, channel_count, interleave, 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IMC;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

/* .IMC in containers */
VGMSTREAM * init_vgmstream_imc_container(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t header_offset, subfile_offset, next_offset, name_offset;
    size_t subfile_size;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "imc"))
        goto fail;

    total_subsongs = read_32bitLE(0x00, streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    header_offset = 0x04 + 0x20*(target_subsong-1);

    name_offset = header_offset + 0x00;
    /* 0x08: flags? (0x702ADE77|0x002ADE77|0x20000000|etc) */
    /* 0x0c: same for all songs in single .imc but varies between .imc */
    subfile_offset = read_32bitLE(header_offset + 0x10,streamFile);
    /* 0x14: flags/size? (0xF0950000|0x3CFA1200|etc) */
    /* 0x18: same for all songs in single .imc but varies between .imc */
    /* 0x1c: flags? (0 or 2) */

    if (target_subsong == total_subsongs) {
        next_offset = get_streamfile_size(streamFile);
    }
    else {
        next_offset = read_32bitLE(header_offset + 0x20 + 0x10,streamFile);
    }
    subfile_size = next_offset - subfile_offset;


    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, NULL);
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_imc(temp_streamFile);
    if (!vgmstream) goto fail;

    close_streamfile(temp_streamFile);
    vgmstream->num_streams = total_subsongs;
    read_string(vgmstream->stream_name,0x08+1, name_offset,streamFile);

    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
