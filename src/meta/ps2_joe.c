#include "meta.h"
#include "../coding/coding.h"

/* .JOE - from Asobo Studio games [Up (PS2), Wall-E (PS2)] */
VGMSTREAM * init_vgmstream_ps2_joe(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, sample_rate;
    int32_t num_samples, loop_start = 0, loop_end = 0;
    size_t file_size, data_size, unknown1, unknown2, interleave;


    /* checks */
    if (!check_extensions(streamFile, "joe"))
        goto fail;

    file_size = get_streamfile_size(streamFile);
    data_size = read_32bitLE(0x04,streamFile);
    unknown1 = read_32bitLE(0x08,streamFile);
    unknown2 = read_32bitLE(0x0c,streamFile);

    /* detect version */
    if (data_size/2 == file_size - 0x10
            && unknown1 == 0x0045039A && unknown2 == 0x00108920) { /* Super Farm (PS2) */
        data_size = data_size / 2;
        interleave = 0x4000;
        start_offset = 0x10;
    }
    else if (data_size/2 == file_size - 0x10
            && unknown1 == 0xCCCCCCCC && unknown2 == 0xCCCCCCCC) { /* Sitting Ducks (PS2) */
        data_size = data_size / 2;
        interleave = 0x8000;
        start_offset = 0x10;
    }
    else if (data_size == file_size - 0x10
            && unknown1 == 0xCCCCCCCC && unknown2 == 0xCCCCCCCC) { /* The Mummy: The Animated Series (PS2) */
        interleave = 0x8000;
        start_offset = 0x10;
    }
    else if (data_size == file_size - 0x4020) { /* CT Special Forces (PS2), and all games beyond */
        interleave = unknown1; /* always 0? */
        if (!interleave)
            interleave = 0x10;
        start_offset = 0x4020; /* header padding contains garbage */
    }
    else {
        goto fail;
    }

    //start_offset = file_size - data_size; /* also ok */
    channel_count = 2;
    sample_rate = read_32bitLE(0x00,streamFile);
    num_samples = ps_bytes_to_samples(data_size, channel_count);


    loop_flag = ps_find_loop_offsets(streamFile, start_offset, data_size, channel_count, interleave,&loop_start, &loop_end);
    /* most songs simply repeat except a few jingles (PS-ADPCM flags are always set) */
    loop_flag = loop_flag && (num_samples > 20*sample_rate); /* in seconds */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_PS2_JOE;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
