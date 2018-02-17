#include "../util.h"
#include "coding.h"

/**
 * IMA ADPCM algorithms (expand one nibble to one sample, based on prev sample/history and step table).
 * Nibbles are usually grouped in blocks/chunks, with a header, containing 1 or N channels
 *
 * All IMAs are mostly the same with these variations:
 * - interleave: blocks and channels are handled externally (layouts) or internally (mixed channels)
 * - block header: none (external), normal (4 bytes of history 16b + step 8b + reserved 8b) or others; per channel/global
 * - expand type: ms-ima style or others; low or high nibble first
 *
 * todo:
 * MS IMAs have the last sample of the prev block in the block header. In Microsoft's implementation, the header sample
 * is written first and last sample is skipped (since they match). vgmstream ignores the header sample and
 * writes the last one instead. This means the very first sample in the first header in a stream is incorrectly skipped.
 * Header step should be 8 bit.
 * Officially defined in "Microsoft Multimedia Standards Update" doc (RIFFNEW.pdf).
 */

static const int ADPCMTable[89] = {
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

static const int IMA_IndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8 
};


/* Standard IMA (most common) */
static void std_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    /* calculate diff = [signed] (step / 8) + (step / 4) + (step / 2) + (step) [when code = 4+2+1]
     * simplified through math, using bitwise ops to avoid rounding:
     *   diff = (code + 1/2) * (step / 4)
     *   > diff = (step * nibble / 4) + (step / 8)
     *     > diff = (((step * nibble) + (step / 2)) / 4) */

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf; /* ADPCM code */
    sample_decoded = *hist1; /* predictor value */
    step = ADPCMTable[*step_index]; /* current step */

    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Apple's IMA variation. Exactly the same except it uses 16b history (probably more sensitive to overflow/sign extend?) */
static void std_ima_expand_nibble_16(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int16_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded); //no need for this actually
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* 3DS IMA (Mario Golf, Mario Tennis; maybe other Camelot games) */
static void n3ds_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    sample_decoded = sample_decoded << 3;
    delta = step * (sample_nibble & 7) * 2 + step; /* custom */
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;
    sample_decoded = sample_decoded >> 3;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* The Incredibles PC, updates step_index before doing current sample */
static void snds_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;

    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;

    step = ADPCMTable[*step_index];

    delta = (sample_nibble & 7) * step / 4 + step / 8; /* standard IMA */
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
}

/* Omikron: The Nomad Soul, algorithm by aluigi */
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
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Ubisoft games, algorithm by Zench (https://bitbucket.org/Zenchreal/decubisnd) */
static void ubi_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = (((sample_nibble & 7) * 2 + 1) * step) >> 3; /* custom */
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* ************************************ */
/* DVI/IMA                              */
/* ************************************ */

/* Standard DVI/IMA ADPCM (as in, ADPCM recommended by the IMA using Intel/DVI's implementation).
 * Configurable: stereo or mono/interleave nibbles, and high or low nibble first.
 * For vgmstream, low nibble is called "IMA ADPCM" and high nibble is "DVI IMA ADPCM" (same thing though). */
void decode_standard_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo, int is_high_first) {
    int i, sample_count = 0;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* external interleave */

    /* no header (external setup), pre-clamp for wrong values */
    if (step_index < 0) step_index=0;
    if (step_index > 88) step_index=88;

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        off_t byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        int nibble_shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high, odd = low */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low, odd = high */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_3ds_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?4:0); //low nibble order

        n3ds_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
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

/* ************************************ */
/* MS-IMA                               */
/* ************************************ */

/* IMA with variable-sized frames, header and custom nibble layout (outputs non-aligned number of samples).
 * Officially defined in "Microsoft Multimedia Standards Update" doc (RIFFNEW.pdf). */
void decode_ms_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, samples_read = 0, samples_done = 0, max_samples;

    int32_t hist1;// = stream->adpcm_history1_32;
    int step_index;// = stream->adpcm_step_index;

    /* internal interleave (configurable size), mixed channels (4 byte per ch) */
    int block_samples = ((vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels) + 1;
    first_sample = first_sample % block_samples;

    /* normal header (hist+step+reserved per channel) */
    {
        off_t header_offset = stream->offset + 0x04*channel;

        hist1 =   read_16bitLE(header_offset+0x00,stream->streamfile);
        step_index = read_8bit(header_offset+0x02,stream->streamfile); /* 0x03: reserved */
        if (step_index < 0) step_index = 0;
        if (step_index > 88) step_index = 88;

        /* write header sample */
        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)hist1;
            samples_done++;
        }
        samples_read++;
    }

    max_samples = (block_samples - samples_read);
    if (max_samples > samples_to_do + first_sample - samples_done)
        max_samples = samples_to_do + first_sample - samples_done; /* for smaller last block */

    /* decode nibbles (layout: alternates 4*2 nibbles per channel) */
    for (i = 0; i < max_samples; i++) {
        off_t byte_offset = stream->offset + 0x04*vgmstream->channels + 0x04*channel + 0x04*vgmstream->channels*(i/8) + (i%8)/2;
        int nibble_shift = (i&1?4:0); /* low nibble first */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);

        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)(hist1);
            samples_done++;
        }
        samples_read++;
    }

    /* internal interleave: increment offset on complete frame */
    if (first_sample + samples_done == block_samples)  {
        stream->offset += vgmstream->interleave_block_size;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_step_index = step_index;
}

/* Reflection's MS-IMA (some layout info from XA2WAV by Deniz Oezmen) */
void decode_ref_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, samples_read = 0, samples_done = 0, max_samples;

    int32_t hist1;// = stream->adpcm_history1_32;
    int step_index;// = stream->adpcm_step_index;

    /* internal interleave (configurable size), mixed channels (4 byte per ch) */
    int block_channel_size = (vgmstream->interleave_block_size - 0x04*vgmstream->channels) / vgmstream->channels;
    int block_samples = ((vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels) + 1;
    first_sample = first_sample % block_samples;

    /* normal header (hist+step+reserved per channel) */
    {
        off_t header_offset = stream->offset + 0x04*channel;

        hist1 =   read_16bitLE(header_offset+0x00,stream->streamfile);
        step_index = read_8bit(header_offset+0x02,stream->streamfile); /* 0x03: reserved */
        if (step_index < 0) step_index = 0;
        if (step_index > 88) step_index = 88;

        /* write header sample */
        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)hist1;
            samples_done++;
        }
        samples_read++;
    }

    max_samples = (block_samples - samples_read);
    if (max_samples > samples_to_do + first_sample - samples_done)
        max_samples = samples_to_do + first_sample - samples_done; /* for smaller last block */

    /* decode nibbles (layout: all nibbles from one channel, then other channels) */
    for (i = 0; i < max_samples; i++) {
        off_t byte_offset = stream->offset + 0x04*vgmstream->channels + block_channel_size*channel + i/2;
        int nibble_shift = (i&1?4:0); /* low nibble first */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);

        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)(hist1);
            samples_done++;
        }
        samples_read++;
    }

    /* internal interleave: increment offset on complete frame */
    if (first_sample + samples_done == block_samples)  {
        stream->offset += vgmstream->interleave_block_size;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_step_index = step_index;
}

/* ************************************ */
/* XBOX-IMA                             */
/* ************************************ */

/* MS-IMA with fixed frame size, skips last sample per channel (for aligment) and custom multichannel nibble layout.
 * For multichannel the layout is (I think) mixed stereo channels (ex. 6ch: 2ch + 2ch + 2ch) */
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
        header_offset = stream->offset + 4*(channel%2);

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int nibble_shift;

        offset = (channelspacing==1) ?
            stream->offset + 4*(channel%2) + 4 + i/8*4 + (i%8)/2 :
            stream->offset + 4*(channel%2) + 4*2 + i/8*4*2 + (i%8)/2;
        nibble_shift = (i&1?4:0); //low nibble first

        std_ima_expand_nibble(stream, offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (channelspacing==1) { /* mono */
        if (offset-stream->offset == 32+3) // ??
            stream->offset += 0x24;
    } else {
        if (offset-stream->offset == 64+(4*(channel%2))+3) // ??
            stream->offset += 0x24*channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* mono XBOX-IMA ADPCM for interleave */
void decode_xbox_ima_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count = 0, num_frame;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave
    int block_samples = (0x24 - 0x4) * 2; /* block size - header, 2 samples per byte */
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    //normal header
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x24*num_frame;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
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
        off_t byte_offset = (stream->offset + 0x24*num_frame + 0x4) + (i-1)/2;
        int nibble_shift = ((i-1)&1?4:0); //low nibble first

        //last nibble/sample in block is ignored (next header sample contains it)
        if (i < block_samples) {
            std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_count] = (short)(hist1);
            sample_count += channelspacing;
        }
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
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

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
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
        step_index = read_8bit(header_offset+2,stream->streamfile);

        //todo clip step_index?
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
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

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
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

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_apple_ima4(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count, num_frame;
    int16_t hist1 = stream->adpcm_history1_16;//todo unneeded 16?
    int step_index = stream->adpcm_step_index;

    //external interleave
    int block_samples = (0x22 - 0x2) * 2;
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    //2-byte header
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x22*num_frame;

        hist1 = (int16_t)((uint16_t)read_16bitBE(header_offset,stream->streamfile) & 0xff80);
        step_index = read_8bit(header_offset+1,stream->streamfile) & 0x7f;
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = (stream->offset + 0x22*num_frame + 0x2) + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        std_ima_expand_nibble_16(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_index;
}

/* XBOX-IMA with modified data layout */
void decode_fsb_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave
    int block_samples = (0x24 - 4) * 2; /* block size - header, 2 samples per byte */
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

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += 36*vgmstream->channels;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* XBOX-IMA with modified data layout */
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
            std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_count] = (short)(hist1);
            sample_count+=channelspacing;
        }
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += vgmstream->interleave_block_size;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}
//todo atenuation: apparently from hcs's analysis Wwise IMA expands nibbles slightly different, reducing clipping/dbs
/*
From Wwise_v2015.1.6_Build5553_SDK.Linux
<_ZN13CAkADPCMCodec12DecodeSampleEiii>:
  10:   83 e0 07                and    $0x7,%eax        ; sample
  13:   01 c0                   add    %eax,%eax        ; sample*2
  15:   83 c0 01                add    $0x1,%eax        ; sample*2+1
  18:   0f af 45 e4             imul   -0x1c(%rbp),%eax ; (sample*2+1)*scale
  1c:   8d 50 07                lea    0x7(%rax),%edx   ; result+7
  1f:   85 c0                   test   %eax,%eax        ; result negative?
  21:   0f 48 c2                cmovs  %edx,%eax        ; adjust if negative to fix rounding for below division
  24:   c1 f8 03                sar    $0x3,%eax        ; (sample*2+1)*scale/8

Different rounding model vs IMA's shift-and-add (also "adjust" step may be unnecessary).
*/

/* MS-IMA with possibly the XBOX-IMA model of even number of samples per block (more tests are needed) */
void decode_awc_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave, mono
    int block_samples = (0x800 - 4) * 2;
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

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += 0x800;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


/* DVI stereo/mono with some mini header and sample output */
void decode_ubi_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count = 0;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave

    //header in the beginning of the stream
    if (stream->channel_start_offset == stream->offset) {
        int version, big_endian, header_samples, max_samples_to_do;
        int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
        off_t offset = stream->offset;

        /* header fields mostly unknown (vary a lot or look like flags),
         * 0x07 0x06 = major/minor tool version?, 0x0c: stereo flag? */
        version = read_8bit(offset + 0x00, stream->streamfile);
        big_endian = version < 5; //todo and sb.big_endian?
        read_16bit = big_endian ? read_16bitBE : read_16bitLE;

        header_samples = read_16bit(offset + 0x0E, stream->streamfile); /* always 10 (per channel) */
        hist1      = read_16bit(offset + 0x10 + channel*0x04,stream->streamfile);
        step_index =  read_8bit(offset + 0x12 + channel*0x04,stream->streamfile);
        offset += 0x10 + 0x08 + 0x04; //todo v6 has extra 0x08?

        /* write PCM samples, must be written to match header's num_samples (hist mustn't) */
        max_samples_to_do = ((samples_to_do > header_samples) ? header_samples : samples_to_do);
        for (i = first_sample; i < max_samples_to_do; i++, sample_count += channelspacing) {
            outbuf[sample_count] = read_16bit(offset + channel*sizeof(sample) + i*channelspacing*sizeof(sample),stream->streamfile);
            first_sample++;
            samples_to_do--;
        }

        /* header done */
        if (i == header_samples) {
            stream->offset = offset + header_samples*channelspacing*sizeof(sample);
        }
    }


    first_sample -= 10; //todo fix hack (needed to adjust nibble offset below)

    for (i = first_sample; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        off_t byte_offset = channelspacing == 1 ?
                stream->offset + i/2 :  /* mono mode */
                stream->offset + i;     /* stereo mode */
        int nibble_shift = channelspacing == 1 ?
                (!(i%2) ? 4:0) :        /* mono mode (high first) */
                (channel==0 ? 4:0);     /* stereo mode (high=L,low=R) */

        ubi_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1); /* all samples are written */
    }

    //external interleave

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


size_t ima_bytes_to_samples(size_t bytes, int channels) {
    /* 2 samples per byte (2 nibbles) in stereo or mono config */
    return bytes * 2 / channels;
}

size_t ms_ima_bytes_to_samples(size_t bytes, int block_align, int channels) {
    /* MS IMA blocks have a 4 byte header per channel; 2 samples per byte (2 nibbles) */
    return (bytes / block_align) * ((block_align - 0x04*channels) * 2 / channels + 1)
            + ((bytes % block_align) ? (((bytes % block_align) - 0x04*channels) * 2 / channels + 1) : 0);
}

size_t xbox_ima_bytes_to_samples(size_t bytes, int channels) {
    int block_align = 0x24 * channels;
    /* XBOX IMA blocks have a 4 byte header per channel; 2 samples per byte (2 nibbles) */
    return (bytes / block_align) * (block_align - 4 * channels) * 2 / channels
            + ((bytes % block_align) ? ((bytes % block_align) - 4 * channels) * 2 / channels : 0); //todo probably not possible (aligned)
}

size_t ubi_ima_bytes_to_samples(size_t bytes, int channels, STREAMFILE *streamFile, off_t offset) {
    int version, big_endian, header_samples;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    size_t header_size = 0;

    version = read_8bit(offset + 0x00, streamFile);
    big_endian = version < 5; //todo and sb.big_endian?
    read_16bit = big_endian ? read_16bitBE : read_16bitLE;

    header_samples = read_16bit(offset + 0x0E, streamFile); /* always 10 (per channel) */
    header_size += 0x10 + 0x04 * channels + 0x04; //todo v6 has extra 0x08?
    header_size += header_samples * channels * sizeof(sample);

    return header_samples + ima_bytes_to_samples(bytes - header_size, channels);
}
