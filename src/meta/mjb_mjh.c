#include "meta.h"
#include "../coding/coding.h"

/* MJB+MJH - SCEE MultiStream? bank of MIB+MIH [Star Wars: Bounty Hunter (PS2)] */
VGMSTREAM* init_vgmstream_mjb_mjh(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sh = NULL;
    off_t start_offset = 0, header_offset = 0;
    size_t data_size, frame_size, frame_count;
    int channels, loop_flag, sample_rate;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "mjb"))
        goto fail;

    sh = open_streamfile_by_ext(sf, "mjh");
    if (!sh) goto fail;

    /* parse entries */
    {
        int i;
        off_t subsong_offset = 0;

        total_subsongs = read_s32le(0x00,sh);
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        for (i = 0; i < total_subsongs; i++) {
            off_t offset = 0x40 + 0x40 * i;
            size_t subsong_size = read_u32le(offset + 0x08,sh) * read_u32le(offset + 0x10,sh) * read_u32le(offset + 0x14,sh);

            if (i + 1== target_subsong) {
                header_offset = offset;
                start_offset = subsong_offset;
            }
            subsong_offset += subsong_size;
        }

        if (!header_offset)
            goto fail;
    }

    VGM_LOG("3: %lx\n", start_offset);

    /* also see MIB+MIH (same thing but this excludes padding stuff) */
    if (read_u32le(header_offset + 0x00,sh) != 0x40)
        goto fail;
    channels        = read_u32le(header_offset + 0x08,sh);
    sample_rate     = read_u32le(header_offset + 0x0c,sh);
    frame_size      = read_u32le(header_offset + 0x10,sh);
    frame_count     = read_u32le(header_offset + 0x14,sh);

    data_size  = frame_count * frame_size * channels;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MJB_MJH;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = data_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    close_streamfile(sh);
    return vgmstream;

fail:
    close_streamfile(sh);
    close_vgmstream(vgmstream);
    return NULL;
}
