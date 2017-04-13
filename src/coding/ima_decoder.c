#include "../util.h"
#include "coding.h"

/**
 * IMA ADPCM algorithms (expand one nibble to one sample, based on prev sample/history and step table).
 * Nibbles are usually grouped in blocks/chunks, with a header, containing 1 or N channels
 *
 * All IMAs are mostly the same with these variations:
 * - interleave: blocks and channels are handled externally (layouts) or internally (mixed channels)
 * - block header: none (external), normal (4 bytes of history 16b + step 16b) or others; per channel/global
 * - expand type: ms-ima style or others; low or high nibble first
 *
 * todo:
 * MS IMAs have the last sample of the prev block in the block header. In Microsoft implementation, the header sample
 * is written first and last sample is skipped (since they match). vgmstream ignores the header sample and
 * writes the last one instead. This means the very first sample in the first header in a stream is incorrectly skipped.
 */

static const int32_t ADPCMTable[89] =
{
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int IMA_IndexTable[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8 
};


/* Original IMA */
static void ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    //"original" ima nibble expansion
    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1 << 3;
    step = ADPCMTable[*step_index];
    delta = step * (sample_nibble & 7) * 2 + step;
    if (sample_nibble & 8)
        sample_decoded -= delta;
    else
        sample_decoded += delta;

    *hist1 = clamp16(sample_decoded >> 3);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Microsoft's IMA (most common) */
static void ms_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];
    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8)
        sample_decoded -= delta;
    else
        sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Apple's MS IMA variation. Exactly the same except it uses 16b history (probably more sensitive to overflow/sign extend) */
static void ms_ima_expand_nibble_16(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int16_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];
    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8)
        sample_decoded -= delta;
    else
        sample_decoded += delta;

    *hist1 = clamp16(sample_decoded); //no need for this actually
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* update step_index before doing current sample */
static void snds_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;

    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;

    step = ADPCMTable[*step_index];
    delta = (sample_nibble & 7) * step / 4 + step / 8;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded = *hist1 + delta;

    *hist1 = clamp16(sample_decoded);
}

/* algorithm by aluigi, unsure if it's a known IMA variation */
static void otns_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];
    delta = 0;
    if(sample_nibble & 4) delta = step << 2;
    if(sample_nibble & 2) delta += step << 1;
    if(sample_nibble & 1) delta += step;
    delta >>= 2;
    if(sample_nibble & 8)
        sample_decoded -= delta;
    else
        sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}


void decode_nds_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_16;//todo unneeded 16?
    int step_index = stream->adpcm_step_index;

    //external interleave

    //normal header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_16bitLE(header_offset+2,stream->streamfile);

        //todo clip step_index?
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_dat4_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_16;//todo unneeded 16?
    int step_index = stream->adpcm_step_index;

    //external interleave

    //normal header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_8bit(header_offset+2,stream->streamfile); //todo use 8bit in all MS IMA?

        //todo clip step_index?
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_ms_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave (configurable size), mixed channels (4 byte per ch)
    int block_samples = (vgmstream->interleave_block_size - 4*vgmstream->channels) * 2 / vgmstream->channels;
    first_sample = first_sample % block_samples;

    //normal header (per channel)
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 4*channel;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_8bit(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4*channel + 4*vgmstream->channels + i/8*4*vgmstream->channels + (i%8)/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += vgmstream->interleave_block_size;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_rad_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave (configurable size), mixed channels (4 byte per ch)
    int block_samples = (vgmstream->interleave_block_size - 4*vgmstream->channels) * 2 / vgmstream->channels;
    first_sample = first_sample % block_samples;

    //inverted header (per channel)
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 4*channel;

        step_index = read_16bitLE(header_offset,stream->streamfile);
        hist1 = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4*vgmstream->channels + channel + i/2*vgmstream->channels;
        int nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += vgmstream->interleave_block_size;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_rad_ima_mono(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //semi-external interleave?
    int block_samples = 0x14 * 2;
    first_sample = first_sample % block_samples;

    //inverted header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        step_index = read_16bitLE(header_offset,stream->streamfile);
        hist1 = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* For multichannel the internal layout is (I think) mixed stereo channels (ex. 6ch: 2ch + 2ch + 2ch)
 * Has extra support for EA blocks, probably could be simplified */
void decode_xbox_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    off_t offset = stream->offset;

    //internal interleave (0x20+4 size), mixed channels (4 byte per ch, mixed stereo)
    int block_samples = (vgmstream->channels==1) ?
            32 :
            32*(vgmstream->channels&2);//todo this can be zero in 4/5/8ch = SEGFAULT using % below
    first_sample = first_sample % block_samples;

    //normal header (per channel)
    if (first_sample == 0) {
        off_t header_offset;
        if(vgmstream->layout_type==layout_ea_blocked) {
            header_offset = stream->offset;
        } else {
            header_offset = stream->offset + 4*(channel%2);
        }

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int nibble_shift;

        if(vgmstream->layout_type==layout_ea_blocked)
            offset = stream->offset + 4 + i/8*4 + (i%8)/2;
        else {
            offset = (channelspacing==1) ?
                stream->offset + 4*(channel%2) + 4 + i/8*4 + (i%8)/2 :
                stream->offset + 4*(channel%2) + 4*2 + i/8*4*2 + (i%8)/2;
        }
        nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if(vgmstream->layout_type==layout_ea_blocked) {
        if(offset-stream->offset==32+3) // ??
            stream->offset+=36;
    } else {
        if(channelspacing==1) {
            if(offset-stream->offset==32+3) // ??
                stream->offset+=36;
        } else {
            if(offset-stream->offset==64+(4*(channel%2))+3) // ??
                stream->offset+=36*channelspacing;
        }
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_int_xbox_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    off_t offset = stream->offset;

    //semi-internal interleave (0x24 size), mixed channels (4 byte per ch)?
    int block_samples = (vgmstream->channels==1) ?
            32 :
            32*(vgmstream->channels&2);//todo this can be zero in 4/5/8ch = SEGFAULT using % below
    first_sample = first_sample % block_samples;

    //normal header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int nibble_shift;

        offset = stream->offset + 4 + i/8*4 + (i%8)/2;
        nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if(channelspacing==1) {
        if(offset-stream->offset==32+3) // ??
            stream->offset+=36;
    } else {
        if(offset-stream->offset==64+(4*(channel%2))+3) // ??
            stream->offset+=36*channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_dvi_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first (old-style DVI)

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_eacs_ima(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL * stream = &(vgmstream->ch[channel]);//todo pass externally for consistency
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    //variable nibble order
    vgmstream->get_high_nibble = !vgmstream->get_high_nibble;
    if((first_sample) && (channelspacing==1))
        vgmstream->get_high_nibble = !vgmstream->get_high_nibble;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i;
        int nibble_shift = (vgmstream->get_high_nibble?0:4); //variable nibble order

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?4:0); //low nibble order

        ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_apple_ima4(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int16_t hist1 = stream->adpcm_history1_16;//todo unneeded 16?
    int step_index = stream->adpcm_step_index;

    off_t packet_offset = stream->offset + first_sample/64*34;

    //semi-internal interleave //todo what
    int block_samples = 64;
    first_sample = first_sample % block_samples;

    //2-byte header
    if (first_sample == 0) {
        hist1 = (int16_t)((uint16_t)read_16bitBE(packet_offset,stream->streamfile) & 0xff80);
        step_index = read_8bit(packet_offset+1,stream->streamfile) & 0x7f;

        //todo no clamp
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = packet_offset + 2 + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble_16(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_snds_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i;//one nibble per channel
        int nibble_shift = (channel==0?0:4); //high nibble first, based on channel

        snds_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_otns_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal/byte interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + (vgmstream->channels==1 ? i/2 : i); //one nibble per channel if stereo
        int nibble_shift = (vgmstream->channels==1) ? //todo simplify
                    (i&1?0:4) : //high nibble first(?)
                    (channel==0?4:0); //low=ch0, high=ch1 (this is correct compared to vids)

        otns_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_fsb_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave
    int block_samples = (36 - 4) * 2; /* block size - header, 2 samples per byte */
    first_sample = first_sample % block_samples;

    //interleaved header (all hist per channel + all step_index per channel)
    if (first_sample == 0) {
        off_t hist_offset = stream->offset + 2*channel;
        off_t step_offset = stream->offset + 2*channel + 2*vgmstream->channels;

        hist1 = read_16bitLE(hist_offset,stream->streamfile);
        step_index = read_8bit(step_offset,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4*vgmstream->channels + 2*channel + i/4*2*vgmstream->channels + (i%4)/2;//2-byte per channel
        int nibble_shift = (i&1?4:0); //low nibble first

        ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += 36*vgmstream->channels;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


void decode_wwise_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count = 0;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave (configurable size), block-interleave multichannel (ex. if block is 0xD8 in 6ch: 6 blocks of 4+0x20)
    int block_samples = (vgmstream->interleave_block_size - 4*vgmstream->channels) * 2 / vgmstream->channels;
    first_sample = first_sample % block_samples;

    //block-interleaved header (1 header per channel block); can be LE or BE
    if (first_sample == 0) {
        int16_t (*read_16bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_16bitBE : read_16bitLE;
        off_t header_offset = stream->offset + (vgmstream->interleave_block_size / vgmstream->channels)*channel;

        hist1 = read_16bit(header_offset,stream->streamfile);
        step_index = read_8bit(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;

        //must write history from header as last nibble/sample in block is almost always 0 / not encoded
        outbuf[sample_count] = (short)(hist1);
        sample_count += channelspacing;
        first_sample += 1;
        samples_to_do -= 1;
    }

    for (i=first_sample; i < first_sample + samples_to_do; i++) { /* first_sample + samples_to_do should be block_samples at most */
        off_t byte_offset = stream->offset + (vgmstream->interleave_block_size / vgmstream->channels)*channel + 4 + (i-1)/2;
        int nibble_shift = ((i-1)&1?4:0); //low nibble first

        //last nibble/sample in block is ignored (next header sample contains it)
        if (i < block_samples) {
            ms_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_count] = (short)(hist1);
            sample_count+=channelspacing;
            //todo atenuation: apparently from hcs's analysis Wwise IMA decodes nibbles slightly different, reducing dbs
        }
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += vgmstream->interleave_block_size;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


size_t ms_ima_bytes_to_samples(size_t bytes, int block_align, int channels) {
    /* MS IMA blocks have a 4 byte header per channel; 2 samples per byte (2 nibbles) */
    return (bytes / block_align) * (block_align - 4 * channels) * 2 / channels;
}
