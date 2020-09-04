#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

static void get_stream_name(char* stream_name, STREAMFILE* sf, int target_stream);

/* SAB - from Worms 4: Mayhem (PC/Xbox/PS2) */
VGMSTREAM* init_vgmstream_sab(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count = 0, is_stream, align, codec, sample_rate, stream_size, loop_start, loop_end;
    int total_subsongs, target_subsong  = sf->stream_index;

    /* .sab: main, .sob: config/names */
    if (!check_extensions(sf,"sab"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x43535732 &&  /* "CSW2" (Windows) */
        read_u32be(0x00,sf) != 0x43535032 &&  /* "CSP2" (PS2) */
        read_u32be(0x00,sf) != 0x43535832)    /* "CSX2" (Xbox) */
        goto fail;

    is_stream = read_u32le(0x04,sf) & 0x04; /* other flags don't seem to matter */
    total_subsongs = is_stream ? 1 : read_u32le(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    align = read_u32le(0x0c,sf); /* doubles as interleave */

    /* stream config */
    codec         = read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x00,sf);
    channel_count = read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x04,sf);
    sample_rate   = read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x08,sf);
    stream_size   = read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x0c,sf);
    loop_start    = read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x10,sf);
    loop_end      = read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x14,sf);
    loop_flag     = (loop_end > 0);

    start_offset  = 0x18 + 0x1c*total_subsongs;
    if (start_offset % align)
        start_offset += align - (start_offset % align);
    start_offset += read_u32le(0x18 + 0x1c*(target_subsong-1) + 0x18,sf);

    if (is_stream) {
        channel_count = read_u32le(0x08,sf); /* uncommon, but non-stream stereo exists */
        stream_size *= channel_count;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SAB;

    switch(codec) {
        case 0x01: /* PC */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = is_stream ? align : 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, vgmstream->channels, 16);
            vgmstream->loop_end_sample = pcm_bytes_to_samples(loop_end, vgmstream->channels, 16);
            break;

        case 0x04: /* PS2 */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = is_stream ? align : 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size, vgmstream->channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, vgmstream->channels);
            vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, vgmstream->channels);
            break;

        case 0x08: /* Xbox */
            vgmstream->coding_type = is_stream ? coding_XBOX_IMA_int : coding_XBOX_IMA;
            vgmstream->layout_type = is_stream ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = is_stream ? align : 0x00;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, vgmstream->channels);
            vgmstream->loop_start_sample = xbox_ima_bytes_to_samples(loop_start, vgmstream->channels);
            vgmstream->loop_end_sample = xbox_ima_bytes_to_samples(loop_end, vgmstream->channels);
            break;

        default:
            VGM_LOG("SAB: unknown codec\n");
            goto fail;
    }

    get_stream_name(vgmstream->stream_name, sf, target_subsong);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* multiple streams may share a name */
static void get_stream_name(char* stream_name, STREAMFILE* sf, int target_stream) {
    STREAMFILE* sf_info = NULL;
    int i, j, total_cues, num_cue = -1;
    size_t name_size = 0;
    off_t name_offset = 0x10;

    sf_info = open_streamfile_by_ext(sf, "sob");
    if (!sf_info) goto end;
    if (read_32bitBE(0x00,sf_info) != 0x43544632) /* "CTF2" */
        goto end;

    total_cues = read_u32le(0x08,sf_info);

    for (i = 0; i < total_cues; i++) {
        uint32_t flags, num_subsections, subsection_1_size, subsection_2_size;

        flags             = read_u32le(name_offset + 0x00,sf_info);
        num_subsections   = read_u32le(name_offset + 0x20,sf_info);
        subsection_1_size = (flags & 0x00000001) ? 0x40 : 0x00;
        subsection_1_size +=(flags & 0x00000040) ? 0x20 : 0x00;
        subsection_2_size = (flags & 0x00000100) ? 0x1c : 0x10;

        for (j = 0; j < num_subsections; j++) {
            int num_stream = read_u32le(name_offset + 0x2c + subsection_1_size + j*subsection_2_size + 0x08,sf_info);
            if (target_stream - 1 == num_stream)
                num_cue = i;
        }

        name_offset += 0x2c + subsection_1_size + subsection_2_size * num_subsections;
    }
    if (num_cue < 0)
        goto end;

    for (i = 0; i < total_cues; i++) {
        /* 0x00: id */
        name_size = read_u32le(name_offset + 0x04,sf_info); /* non null-terminated */
        if (i == num_cue) {
            name_offset += 0x08;
            break;
        }
        name_offset += 0x08 + name_size;
    }

    if (name_size > STREAM_NAME_SIZE-1)
        name_size = STREAM_NAME_SIZE-1;

    read_string(stream_name,name_size+1, name_offset,sf_info);

end:
    close_streamfile(sf_info);
}
