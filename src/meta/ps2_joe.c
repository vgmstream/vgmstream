#include "meta.h"
#include "../coding/coding.h"

static size_t joe_find_padding(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave);

/* .JOE - from Asobo Studio games [Up (PS2), Wall-E (PS2)] */
VGMSTREAM* init_vgmstream_ps2_joe(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, sample_rate;
    int32_t num_samples;
    size_t file_size, data_size, unknown1, unknown2, interleave, padding_size;


    /* checks */
    if (!check_extensions(sf, "joe"))
        goto fail;

    file_size = get_streamfile_size(sf);
    data_size = read_u32le(0x04,sf);
    unknown1 = read_u32le(0x08,sf);
    unknown2 = read_u32le(0x0c,sf);

    /* detect version */
    if (data_size == file_size - 0x800
            && unknown1 == 0x00002000 && unknown2 == 0xFFFFFFFF) { /* NYR (PS2) */
        interleave = 0x2000;
        start_offset = 0x800;
    }
    else if (data_size / 2 == file_size - 0x10
            && unknown1 == 0x0045039A && unknown2 == 0x00108920) { /* Super Farm (PS2) */
        data_size = data_size / 2;
        interleave = 0x4000;
        start_offset = 0x10;
    }
    else if (data_size / 2 == file_size - 0x10
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
    else if (data_size == file_size - 0x4020) { /* Counter Terrorism Special Forces (PS2), all games beyond */
        /* header can be section(?) table (0x08=entry count) then 0xCCCCCCCC, all 0s, or all 0xCCCCCCCC (no table) */
        interleave = 0x10;
        start_offset = 0x4020;
    }
    else {
        goto fail;
    }

    //start_offset = file_size - data_size; /* also ok */
    channel_count = 2;
    loop_flag = 0;
    sample_rate = read_s32le(0x00,sf);

    /* the file's end is padded with either 0xcdcdcdcd or zeroes (but not always, ex. NYR) */
    padding_size = joe_find_padding(sf, start_offset, data_size, channel_count, interleave);
    data_size -= padding_size;
    num_samples = ps_bytes_to_samples(data_size, channel_count);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->stream_size = data_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_PS2_JOE;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static size_t joe_find_padding(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave) {
    uint32_t pad;
    off_t min_offset, offset;
    size_t frame_size = 0x10;
    size_t padding_size = 0;
    size_t interleave_consumed = 0;

    if (data_size == 0 || channels == 0 || (channels > 0 && interleave == 0))
        return 0;

    offset = start_offset + data_size - interleave * (channels - 1);
    min_offset = start_offset;

    while (offset > min_offset) {
        offset -= frame_size;
        pad = read_u32be(offset, sf);
        if (pad != 0xCDCDCDCD && pad != 0x00000000)
            break;

        padding_size += frame_size * channels;

        /* skip other channels */
        interleave_consumed += 0x10;
        if (interleave_consumed == interleave) {
            interleave_consumed = 0;
            offset -= interleave * (channels - 1);
        }
    }

    if (padding_size >= data_size)
        return 0;

    return padding_size;
}
