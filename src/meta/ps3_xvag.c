#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

static int ps_adpcm_find_loop_offsets(STREAMFILE *streamFile, int channel_count, off_t start_offset, off_t * loop_start, off_t * loop_end);

/* XVAG - Sony's (second party?) format (God of War III, Ratchet & Clank Future, The Last of Us, Uncharted) */
VGMSTREAM * init_vgmstream_ps3_xvag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int loop_flag = 0, channel_count, codec;

    off_t start_offset, loop_start, loop_end, chunk_offset;
    off_t first_offset = 0x20;
    int little_endian;
    int sample_rate, num_samples, multiplier;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"xvag")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58564147) /* "XVAG" */
        goto fail;

    little_endian = read_8bit(0x07,streamFile)==0; /* empty start_offset > little endian */
    if (little_endian) {
        read_32bit = read_32bitLE;
    } else {
        read_32bit = read_32bitBE;
    }

    start_offset = read_32bit(0x04,streamFile);
    /* 0x08: flags? (&0x01=big endian?)  0x0a: version (chunk sizes vary) */

    /* "fmat": base format */
    if (!find_chunk(streamFile, 0x666D6174,first_offset,0, &chunk_offset,NULL, !little_endian, 1)) goto fail; /*"fmat"*/
    channel_count = read_32bit(chunk_offset+0x00,streamFile);
    codec = read_32bit(chunk_offset+0x04,streamFile);
    num_samples = read_32bit(chunk_offset+0x08,streamFile);
    /* 0x0c: samples again? */
    multiplier = read_32bit(chunk_offset+0x10,streamFile);
    sample_rate = read_32bit(chunk_offset+0x14,streamFile);
    /* 0x18: datasize */

    /* other chunks: */
    /* "cpan": pan/volume per channel */
    /* "0000": end chunk before start_offset */

    //if ((uint16_t)read_16bitBE(start_offset,streamFile)==0xFFFB) codec = 0x08;
    if (codec == 0x06) { /* todo not sure if there are any looping XVAGs */
        loop_flag = ps_adpcm_find_loop_offsets(streamFile, channel_count, start_offset, &loop_start, &loop_end);
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->meta_type = meta_PS3_XVAG;

    switch (codec) {
        case 0x06:   /* PS ADPCM: God of War III, Uncharted 1/2, Ratchet and Clank Future */
        case 0x07: { /* Bizarro 6ch PS ADPCM: infamous 1 (todo won't play properly; algo tweak + bigger predictor table?) */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;//* multiplier? (doesn't seem necessary, always 1);
            vgmstream->coding_type = coding_PSX;

            if (loop_flag) {
                if (loop_start!=0) {
                    vgmstream->loop_start_sample = ((((loop_start/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*28)/channel_count;
                    if(loop_start%vgmstream->interleave_block_size)
                        vgmstream->loop_start_sample += (((loop_start%vgmstream->interleave_block_size)-1)/16*14*channel_count);
                }
                vgmstream->loop_end_sample = ((((loop_end/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*28)/channel_count;
                if (loop_end%vgmstream->interleave_block_size)
                    vgmstream->loop_end_sample += (((loop_end%vgmstream->interleave_block_size)-1)/16*14*channel_count);
            }

            break;
        }

#ifdef VGM_USE_MPEG
        case 0x08: { /* MPEG: The Last of Us, Uncharted 3, Medieval Moves */
            mpeg_codec_data *mpeg_data = NULL;
            coding_t mpeg_coding_type;
            int fixed_frame_size;

            /* "mpin": mpeg info */
            /*  0x00/04: mpeg version/layer?  other: unknown or repeats of "fmat" */
            if (!find_chunk(streamFile, 0x6D70696E,first_offset,0, &chunk_offset,NULL, !little_endian, 1)) goto fail; /*"mpin"*/
            fixed_frame_size = read_32bit(chunk_offset+0x1c,streamFile);

            mpeg_data = init_mpeg_codec_data_interleaved(streamFile, start_offset, &mpeg_coding_type, vgmstream->channels, fixed_frame_size, 0);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;
            vgmstream->layout_type = layout_mpeg;
            vgmstream->coding_type = mpeg_coding_type;
            vgmstream->interleave_block_size = fixed_frame_size * multiplier;

            break;
        }
#endif

        case 0x09: { /* ATRAC9: Sly Cooper and the Thievius Raccoonus */
            /* "a9in": ATRAC9 info */
            goto fail;
        }

        default:
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


static int ps_adpcm_find_loop_offsets(STREAMFILE *streamFile, int channel_count, off_t start_offset, off_t * loop_start, off_t * loop_end) {
    uint8_t testBuffer[0x10];
    int     loopStartPointsCount=0;
    int     loopEndPointsCount=0;
    off_t   readOffset = 0;
    off_t   loopStartPoints[0x10];
    off_t   loopEndPoints[0x10];

    off_t   loopStart = 0;
    off_t   loopEnd = 0;
    off_t fileLength;
    int loop_flag = 0;

    readOffset=start_offset;
    fileLength = get_streamfile_size(streamFile);

    // get the loops the same way we get on .MIB
    do {
        readOffset+=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile);

        // Loop Start ...
        if(testBuffer[0x01]==0x06) {
            if(loopStartPointsCount<0x10) {
                loopStartPoints[loopStartPointsCount] = readOffset-0x10;
                loopStartPointsCount++;
            }
        }

        // Loop End ...
        if(((testBuffer[0x01]==0x03) && (testBuffer[0x03]!=0x77)) || (testBuffer[0x01]==0x01)) {
            if(loopEndPointsCount<0x10) {
                loopEndPoints[loopEndPointsCount] = readOffset; //-0x10;
                loopEndPointsCount++;
            }
        }

    } while (readOffset<((int32_t)fileLength));

    // Calc Loop Points & Interleave ...
    if(loopStartPointsCount>=channel_count) {
        // can't get more then 0x10 loop point !
        if((loopStartPointsCount<=0x0F) && (loopStartPointsCount>=2)) {
            // Always took the first 2 loop points
            loopStart=loopStartPoints[1]-start_offset;
            loop_flag=1;
        } else {
            loopStart=0;
        }
    }

    if(loopEndPointsCount>=channel_count) {
        // can't get more then 0x10 loop point !
        if((loopEndPointsCount<=0x0F) && (loopEndPointsCount>=2)) {
            loop_flag=1;
            loopEnd=loopEndPoints[loopEndPointsCount-1]-start_offset;
        } else {
            loopEnd=0;
        }
    }

    // as i can get on the header if a song is looped or not
    // if try to take the loop marker on the file
    // if the last byte of the file is = 00 is assume that the song is not looped
    // i know that i can cover 95% of the file, but can't work on some of them
    if(read_8bit((fileLength-1),streamFile)==0)
        loop_flag=0;

    if (loop_flag) {
        *loop_start = loopStart;
        *loop_end = loopEnd;
    }

    return loop_flag;
}
