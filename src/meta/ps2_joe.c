#include "meta.h"
#include "../coding/coding.h"

/* .JOE - from Asobo Studio games [Up (PS2), Wall-E (PS2)] */
VGMSTREAM * init_vgmstream_ps2_joe(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, sample_rate, num_samples;
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
            && unknown1 == 0x0045039A && unknown2 == 0x00108920) { /* Super Farm */
        data_size = data_size / 2;
        interleave = 0x4000;
    }
    else if (data_size/2 == file_size - 0x10
            && unknown1 == 0xCCCCCCCC && unknown2 == 0xCCCCCCCC) { /* Sitting Ducks */
        data_size = data_size / 2;
        interleave = 0x8000;
    }
    else if (data_size == file_size - 0x10
            && unknown1 == 0xCCCCCCCC && unknown2 == 0xCCCCCCCC) { /* The Mummy: The Animated Series */
        interleave = 0x8000;
    }
    else if (data_size == file_size - 0x4020) { /* CT Special Forces (and all games beyond) */
        interleave = unknown1; /* always 0? */
        if (!interleave)
            interleave = 0x10;
        /* header padding contains garbage */
    }
    else {
        goto fail;
    }

    start_offset = file_size - data_size;

    channel_count = 2;
    sample_rate = read_32bitLE(0x00,streamFile);
    num_samples = ps_bytes_to_samples(data_size, channel_count);

    /* most songs simply repeat except a few jingles (PS-ADPCM flags are always set) */
    loop_flag = (num_samples > 20*sample_rate); /* in seconds */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    //todo improve, not working 100% with early .joe
    {
        uint8_t testBuffer[0x10];
        off_t blockOffset = 0;
        off_t sampleOffset = 0;
        off_t readOffset = 0;
        off_t loopStart = 0, loopEnd = 0;

        readOffset = start_offset;
        do {
            off_t blockRead = (off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile);

            readOffset += blockRead;
            blockOffset += blockRead;

            if (blockOffset >= interleave) {
                readOffset += interleave;
                blockOffset -= interleave;
            }

            /* Loop Start */
            if(testBuffer[0x01]==0x06) {
                if(loopStart == 0)
                    loopStart = sampleOffset;
                /* break; */
            }

            sampleOffset += 28;

            /* Loop End */
            if(testBuffer[0x01]==0x03) {
                if(loopEnd == 0)
                    loopEnd = sampleOffset;
                /* break; */
            }

        } while (streamFile->get_offset(streamFile)<(int32_t)file_size);

        if (loopStart == 0 && loopEnd == 0) {
            vgmstream->loop_flag = 0;
        } else {
            vgmstream->loop_start_sample = loopStart;
            vgmstream->loop_end_sample = loopEnd;
        }
    }

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
