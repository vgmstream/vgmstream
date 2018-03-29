#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

static void get_stream_name(char * stream_name, STREAMFILE *streamFile, int target_stream);

/* SAB - from Worms 4: Mayhem (PC/Xbox/PS2) */
VGMSTREAM * init_vgmstream_sab(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count = 0, is_stream, align, codec, sample_rate, stream_size, loop_start, loop_end;
    int total_subsongs, target_subsong  = streamFile->stream_index;

    /* .sab: main, .sob: config/names */
    if (!check_extensions(streamFile,"sab"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x43535732 &&  /* "CSW2" (Windows) */
        read_32bitBE(0x00,streamFile) != 0x43535032 &&  /* "CSP2" (PS2) */
        read_32bitBE(0x00,streamFile) != 0x43535832)    /* "CSX2" (Xbox) */
        goto fail;

    is_stream = read_32bitLE(0x04,streamFile) & 0x04; /* other flags don't seem to matter */
    total_subsongs = is_stream ? 1 : read_32bitLE(0x08,streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    align = read_32bitLE(0x0c,streamFile); /* doubles as interleave */

    /* stream config */
    codec         = read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x00,streamFile);
    channel_count = read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x04,streamFile);
    sample_rate   = read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x08,streamFile);
    stream_size   = read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x0c,streamFile);
    loop_start    = read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x10,streamFile);
    loop_end      = read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x14,streamFile);
    loop_flag     = (loop_end > 0);

    start_offset  = 0x18 + 0x1c*total_subsongs;
    if (start_offset % align)
        start_offset += align - (start_offset % align);
    start_offset += read_32bitLE(0x18 + 0x1c*(target_subsong-1) + 0x18,streamFile);

    if (is_stream) {
        channel_count = read_32bitLE(0x08,streamFile); /* uncommon, but non-stream stereo exists */
        stream_size *= channel_count;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
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

    get_stream_name(vgmstream->stream_name, streamFile, target_subsong);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* multiple streams may share a name, also sometimes won't a the name */
static void get_stream_name(char * stream_name, STREAMFILE *streamFile, int target_stream) {
    STREAMFILE * streamInfo = NULL;
    int i, j, total_cues, num_cue = -1;
    size_t name_size = 0;
    off_t name_offset = 0x10;

    streamInfo = open_streamfile_by_ext(streamFile, "sob");
    if (!streamInfo) goto end;
    if (read_32bitBE(0x00,streamInfo) != 0x43544632) /* "CTF2" */
        goto end;

    total_cues = read_32bitLE(0x08,streamInfo);

    for (i = 0; i < total_cues; i++) {
        uint32_t flags, num_subsections, subsection_1_size, subsection_2_size;

        flags             = (uint32_t)read_32bitLE(name_offset + 0x00,streamInfo);
        num_subsections   = (uint32_t)read_32bitLE(name_offset + 0x20,streamInfo);
        subsection_1_size = (flags & 0x00000001) ? 0x40 : 0x00;
        subsection_1_size +=(flags & 0x00000040) ? 0x20 : 0x00;
        subsection_2_size = (flags & 0x00000100) ? 0x1c : 0x10;

        for (j = 0; j < num_subsections; j++) {
            int num_stream = read_32bitLE(name_offset + 0x2c + subsection_1_size + j*subsection_2_size + 0x08,streamInfo);
            if (target_stream-1 == num_stream)
                num_cue = i;
        }

        name_offset += 0x2c + subsection_1_size + subsection_2_size * num_subsections;
    }
    if (num_cue < 0)
        goto end;

    for (i = 0; i < total_cues; i++) {
        /* 0x00: id */
        name_size = read_32bitLE(name_offset + 0x04,streamInfo); /* non null-terminated */
        if (i == num_cue) {
            name_offset += 0x08;
            break;
        }
        name_offset += 0x08 + name_size;
    }

    if (name_size > STREAM_NAME_SIZE-1)
        name_size = STREAM_NAME_SIZE-1;

    read_string(stream_name,name_size+1, name_offset,streamInfo);

end:
    if (streamInfo)
        close_streamfile(streamInfo);
}
