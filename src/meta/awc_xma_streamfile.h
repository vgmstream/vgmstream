#ifndef _AWC_XMA_STREAMFILE_H_
#define _AWC_XMA_STREAMFILE_H_
#include "deblock_streamfile.h"


static size_t get_block_header_size(STREAMFILE* sf, off_t offset, int channels);
static size_t get_repeated_data_size(STREAMFILE* sf, off_t next_offset, size_t repeat_samples);
static size_t get_block_skip_count(STREAMFILE* sf, off_t offset, int channel);

static void block_callback(STREAMFILE *sf, deblock_io_data* data) {
    const size_t frame_size = 0x800;
    int channel = data->cfg.track_number;
    int channels = data->cfg.track_count;

    /* Blocks have a header then data per channel, each with a different num_samples/frames, 
     * separate (first all frames of ch0, then ch1, etc), padded, and sometimes the last few
     * frames of a channel are repeated in the new block (marked with "repeat samples"). */
    size_t header_size    = get_block_header_size(sf, data->physical_offset, channels);
    /* header table entries = frames... I hope */
    size_t others_size    = get_block_skip_count(sf, data->physical_offset, channel) * frame_size;
  //size_t skip_size      = read_u32be(data->physical_offset + 0x10*channel + 0x00, sf) * frame_size;
    size_t data_size      = read_u32be(data->physical_offset + 0x10*channel + 0x04, sf) * frame_size;
    size_t repeat_samples = read_u32be(data->physical_offset + 0x10*channel + 0x08, sf);
    size_t repeat_size    = 0;

    data->block_size = data->cfg.chunk_size;

    /* if there are repeat samples current block repeats some frames from last block, find out size */
    if (repeat_samples) {
        off_t data_offset = data->physical_offset + header_size + others_size;
        repeat_size = get_repeated_data_size(sf, data_offset, repeat_samples);
    }

    data->skip_size = header_size + others_size + repeat_size;
    data->data_size = data_size - repeat_size;
}

/* block header size, aligned/padded to 0x800 */
static size_t get_block_header_size(STREAMFILE* sf, off_t offset, int channels) {
    size_t header_size = 0;
    int i;

    for (i = 0; i < channels; i++) {
        header_size += 0x10;
        header_size += read_u32be(offset + 0x10*i + 0x04, sf) * 0x04; /* entries in the table */
    }

    if (header_size % 0x800) /* padded */
        header_size +=  0x800 - (header_size % 0x800);

    return header_size;
}

/* find data that repeats in the beginning of a new block at the end of last block */
static size_t get_repeated_data_size(STREAMFILE* sf, off_t next_offset, size_t repeat_samples) {
    const size_t frame_size = 0x800;
    const size_t samples_per_subframe = 512;
    size_t samples_this_frame;
    uint8_t subframes;

    //todo: fix this
    /* Repeat samples are the number of decoded samples to discard, but in this streamfile we can't do that.
     * Also XMA is VBR, and may encode silent frames with up to 63 subframes yet we may have few repeat samples.
     * We could find out how many subframes of 512 samples to skip, then adjust the XMA frame header, though
     * output will be slightly off since subframes are related.
     *
     * For now just skip a full frame depending on the number of subframes vs repeat samples.
     * Most files work ok-ish but channels may desync slightly. */

    subframes = ((uint8_t)read_8bit(next_offset,sf) >> 2) & 0x3F; /* peek into frame header */
    samples_this_frame = subframes*samples_per_subframe;
    if (repeat_samples >= (int)(samples_this_frame*0.13)) { /* skip mosts */
        return frame_size;
    }
    else {
        return 0;
    }
}

/* header has a skip value, but somehow it's sometimes bigger than expected (WHY!?!?) so just sum all */
static size_t get_block_skip_count(STREAMFILE* sf, off_t offset, int channel) {
    size_t skip_count = 0;
    int i;

    //skip_size = read_u32be(offset + 0x10*channel + 0x00, sf); /* wrong! */
    for (i = 0; i < channel; i++) {
        skip_count += read_u32be(offset + 0x10*i + 0x04, sf); /* number of frames of this channel */
    }

    return skip_count;
}


/* Deblocks interleaved XMA in AWC blocks */
static STREAMFILE* setup_awc_xma_streamfile(STREAMFILE *sf, off_t stream_offset, size_t stream_size, size_t block_size, int channel_count, int channel) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.track_number = channel;
    cfg.track_count = channel_count;
    cfg.stream_start = stream_offset;
    cfg.stream_size = stream_size;
    cfg.chunk_size = block_size;
    //cfg.physical_offset = stream_offset;
    //cfg.logical_size = awc_xma_io_size(sf, &cfg); /* force init */
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    return new_sf;
}

#endif /* _AWC_XMA_STREAMFILE_H_ */
