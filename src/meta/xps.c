#include "meta.h"
#include "../coding/coding.h"

static void read_xps_name(VGMSTREAM *vgmstream, STREAMFILE *streamFile, int file_id);

/* .XPS+DAT - From Software games streams [Metal Wolf Chaos (Xbox), Otogi (Xbox)] */
VGMSTREAM * init_vgmstream_xps_dat(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;
    off_t start_offset, header_offset;
    size_t stream_size;
    int loop_flag, channel_count, sample_rate, codec, loop_start_sample, loop_end_sample, file_id;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "xps"))
        goto fail;

    if (read_32bitLE(0x00,streamFile) != get_streamfile_size(streamFile))
        goto fail;
    if (read_32bitBE(0x0c,streamFile) != 0x64696666)  /* "diff" */
        goto fail;

    /* handle .xps+dat (bank .xps are done below) */
    streamData = open_streamfile_by_ext(streamFile, "dat");
    if (!streamData) goto fail;

    /* 0x00: approximate file size */

    total_subsongs = read_32bitLE(0x04,streamData);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = 0x20 + 0x94*(target_subsong-1); /* could start at 0x0c too */

    file_id             = read_32bitLE(header_offset+0x00,streamData);
    start_offset        = read_32bitLE(header_offset+0x04,streamData);
    stream_size         = read_32bitLE(header_offset+0x08,streamData);
    /* 0x0c: loop start offset? */
    /* 0x10: loop end offset? */
    /* 0x14: always null? */
    codec               = read_16bitLE(header_offset+0x18,streamData);
    channel_count       = read_16bitLE(header_offset+0x1a,streamData);
    sample_rate         = read_32bitLE(header_offset+0x1c,streamData);
    /* 0x20: average bitrate */
    /* 0x24: block size, bps */
    loop_flag           = read_32bitLE(header_offset+0x5c,streamData);
    loop_start_sample   = read_32bitLE(header_offset+0x6c,streamData);
    loop_end_sample     = read_32bitLE(header_offset+0x70,streamData) + 1; /* a "smpl" chunk basically */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_XPS_DAT;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x01:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channel_count, 16);
            break;

        case 0x69:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channel_count);
            break;

        default:
            goto fail;
    }

    read_xps_name(vgmstream, streamFile, file_id);

    if (!vgmstream_open_stream(vgmstream,streamData,start_offset))
        goto fail;

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}

static void read_xps_name(VGMSTREAM *vgmstream, STREAMFILE *streamFile, int file_id) {
    int i, entries;
    int name_id = -1, udss_name_id;
    off_t entry_offset = 0x10;


    /* main section + stream sections (usually same number but not always) */
    entries = read_32bitLE(0x04,streamFile);

    /* "sid\0" entries: find name_id of file_id */
    for (i = 0; i < entries; i++) {
        off_t entry_base  = entry_offset;
        size_t entry_size = read_32bitLE(entry_base+0x00,streamFile);
        uint32_t entry_id = read_32bitBE(entry_base+0x04,streamFile);
        size_t entry_pad  = read_32bitLE(entry_base+0x08,streamFile);
        /* 0x0c: always null, rest: entry (format varies) */

        entry_offset += entry_size + entry_pad + 0x10;

        /* sound info entry */
        if (entry_id == 0x73696400) { /* "sid\0" */
            int entry_file_id = read_32bitLE(entry_base+0x10,streamFile);
            int entry_name_id = read_32bitLE(entry_base+0x14,streamFile);
            if (entry_file_id == file_id && name_id == -1) {
                name_id = entry_name_id;
            }
            continue;
        }

        /* sound stream entry, otherwise no good */
        if (entry_id != 0x75647373) { /* "udss" */
            goto fail;
        }

        udss_name_id = read_32bitLE(entry_base+0x10,streamFile);
        if (udss_name_id == name_id) {
            off_t name_offset = entry_base + 0x10 + 0x08;
            size_t name_size = entry_size - 0x08; /* includes null */
            read_string(vgmstream->stream_name,name_size, name_offset,streamFile);
            return;
        }
    }

fail:
    return;
}

/* .XPS - From Software games banks [Metal Wolf Chaos (Xbox), Otogi (Xbox)] */
VGMSTREAM * init_vgmstream_xps(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;
    int i, entries;
    off_t entry_offset = 0x10;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "xps"))
        goto fail;

    if (read_32bitLE(0x00,streamFile) != get_streamfile_size(streamFile))
        goto fail;
    if (read_32bitBE(0x0c,streamFile) != 0x64696666)  /* "diff" */
        goto fail;

    /* handle .xps alone (stream .xps+data are done above) */
    streamData = open_streamfile_by_ext(streamFile, "dat");
    if (streamData) goto fail;

    /* main section + bank sections (usually same number but not always) */
    entries = read_32bitLE(0x04,streamFile);

    total_subsongs = 0;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 /*|| target_subsong > total_subsongs || total_subsongs < 1*/) goto fail;


    /* parse entries: skip (there is probably a stream/bank flag here) */
    for (i = 0; i < entries; i++) {
        off_t entry_base  = entry_offset;
        size_t entry_size = read_32bitLE(entry_base+0x00,streamFile);
        uint32_t entry_id = read_32bitBE(entry_base+0x04,streamFile);
        size_t entry_pad  = read_32bitLE(entry_base+0x08,streamFile);
        /* 0x0c: always null, rest: entry (format varies) */

        entry_offset += entry_size + entry_pad + 0x10;

        /* sound info entry */
        if (entry_id == 0x73696400) { /* "sid\0" */
            /* keep looking for sound banks */
            continue;
        }

        /* sound bank entry, otherwise no good */
        if (entry_id != 0x75647362) { /* "udsb" */
            goto fail;
        }

        total_subsongs++;

        /* open internal RIFF */
        if (target_subsong == total_subsongs && vgmstream == NULL) {
            STREAMFILE* temp_streamFile;
            off_t subsong_offset = entry_base+0x18;
            size_t subsong_size  = read_32bitLE(entry_base+0x14,streamFile);

            temp_streamFile = setup_subfile_streamfile(streamFile, subsong_offset,subsong_size, "wav");
            if (!temp_streamFile) goto fail;

            vgmstream = init_vgmstream_riff(temp_streamFile);
            close_streamfile(temp_streamFile);
            if (!vgmstream) goto fail;

        }
    }

    /* subsong not found */
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_subsongs;
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}
