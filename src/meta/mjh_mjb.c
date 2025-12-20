#include "meta.h"
#include "../coding/coding.h"

/* MJH+MJB - SCEE MultiStream? bank of MIH+MIB [Star Wars: Bounty Hunter (PS2)] */
VGMSTREAM* init_vgmstream_mjh_mjb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    off_t start_offset = 0, header_offset = 0;
    size_t data_size, frame_size, frame_count;
    int channels, loop_flag, sample_rate;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    /* "base" header check only has total subsongs, check expected header size */
    if (read_s32le(0x00,sf) * 0x40 + 0x40 != get_streamfile_size(sf))
        return NULL;
    if (read_u32le(0x10,sf) != 0 || read_u32le(0x20,sf) != 0 || read_u32le(0x30,sf) != 0) // padding until 0x40
        return NULL;
    if (!check_extensions(sf, "mjh"))
        return NULL;


    /* parse entries */
    {
        off_t subsong_offset = 0;

        total_subsongs = read_s32le(0x00,sf);
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        for (int i = 0; i < total_subsongs; i++) {
            off_t offset = 0x40 + 0x40 * i;
            size_t subsong_size = read_u32le(offset + 0x08,sf) * read_u32le(offset + 0x10,sf) * read_u32le(offset + 0x14,sf);

            if (i + 1== target_subsong) {
                header_offset = offset;
                start_offset = subsong_offset;
            }
            subsong_offset += subsong_size;
        }

        if (!header_offset)
            return NULL;
    }

    /* also see MIH+MIB (same thing but this excludes padding stuff) */
    if (read_u32le(header_offset + 0x00,sf) != 0x40)
        return NULL;
    channels        = read_u32le(header_offset + 0x08,sf);
    sample_rate     = read_u32le(header_offset + 0x0c,sf);
    frame_size      = read_u32le(header_offset + 0x10,sf);
    frame_count     = read_u32le(header_offset + 0x14,sf);

    data_size  = frame_count * frame_size * channels;
    loop_flag = 0;

    sb = open_streamfile_by_ext(sf, "mjb");
    if (!sb) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MJH_MJB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = data_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    if (sf != sb) close_streamfile(sb);
    return vgmstream;

fail:
    if (sf != sb) close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
