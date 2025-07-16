#include "meta.h"
#include "../coding/coding.h"


/* Headerless PS-ADPCM
 * Guesses interleave/channels/loops by testing data and using the file extension for sample rate.
 * This is an ugly crutch for older sets, use TXTH to properly play headerless data instead. */
VGMSTREAM * init_vgmstream_ps_headerless(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0x00;
    char filename[PATH_LIMIT];

    uint8_t mibBuffer[0x10];
    uint8_t testBuffer[0x10];

    size_t  fileLength;
    off_t   loopStart = 0;
    off_t   loopEnd = 0;
    off_t   interleave = 0;

    off_t   readOffset = 0;

    off_t   loopStartPoints[0x10] = {0};
    int     loopStartPointsCount=0;
    off_t   loopEndPoints[0x10] = {0};
    int     loopEndPointsCount=0;
    int     loopToEnd=0;
    int     forceNoLoop=0;
    int     gotEmptyLine=0;

    int i, channel_count=0;


    /* checks
     * .mib: common, but many ext-less files are renamed to this.
     * .mi4: fake .mib to force another sample rate */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mib",filename_extension(filename)) && 
        strcasecmp("mi4",filename_extension(filename)))
        goto fail;

    /* test if raw PS-ADPCM */
    if (!ps_check_format(streamFile, 0x00, 0x2000))
        goto fail;


    fileLength = get_streamfile_size(streamFile);

    /* Search for interleave value (checking channel starts) and loop points (using PS-ADPCM flags).
     * Channel start will by 0x0000, 0x0002, 0x0006 followed by 12 zero values.
     * Interleave value is the offset where those repeat, and channels the number of times.
     * Loop flags in second byte are: 0x06 = start, 0x03 = end (per channel).
     * Interleave can be large (up to 0x20000 found so far) and is always a 0x10 multiple value. */
    readOffset+=(off_t)read_streamfile(mibBuffer,0,0x10,streamFile);
    mibBuffer[0]=0;
    {
        uint8_t doChannelUpdate=1;
        uint8_t bDoUpdateInterleave=1;

        readOffset=0;
        do {
            readOffset+=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile);
            // be sure to point to an interleave value
            if(readOffset<(int32_t)(fileLength*0.5)) {

                if(memcmp(testBuffer+2, mibBuffer+2,0x0e)) {
                    if(doChannelUpdate) {
                        doChannelUpdate=0;
                        channel_count++;
                    }
                    if(channel_count<2)
                        bDoUpdateInterleave=1;
                }

                testBuffer[0]=0;
                if(!memcmp(testBuffer,mibBuffer,0x10)) {
                    gotEmptyLine=1;

                    if(bDoUpdateInterleave) {
                        bDoUpdateInterleave=0;
                        interleave=readOffset-0x10;
                    }
                    if(readOffset-0x10 == channel_count*interleave) {
                        doChannelUpdate=1;
                    }
                }
            }

            // Loop Start ...
            if(testBuffer[0x01]==0x06) {
                if(loopStartPointsCount<0x10) {
                    loopStartPoints[loopStartPointsCount] = readOffset-0x10;
                    loopStartPointsCount++;
                }
            }

            // Loop End ...
            if(testBuffer[0x01]==0x03 && testBuffer[0x03]!=0x77) {
                if(loopEndPointsCount<0x10) {
                    loopEndPoints[loopEndPointsCount] = readOffset;
                    loopEndPointsCount++;
                }
            }

            if(testBuffer[0x01]==0x04) {
                // 0x04 loop points flag can't be with a 0x03 loop points flag
                if(loopStartPointsCount<0x10) {
                    loopStartPoints[loopStartPointsCount] = readOffset-0x10;
                    loopStartPointsCount++;

                    // Loop end value is not set by flags ...
                    // go until end of file
                    loopToEnd=1;
                }
            }

        } while (readOffset<((int32_t)fileLength));
    }

    if(testBuffer[0]==0x0c && testBuffer[1]==0)
        forceNoLoop=1;

    if(channel_count==0)
        channel_count=1;

    // Calc Loop Points & Interleave ...
    if(loopStartPointsCount>=2) {
        // can't get more then 0x10 loop point !
        if(loopStartPointsCount<=0x0F) {
            // Always took the first 2 loop points
            interleave=loopStartPoints[1]-loopStartPoints[0];
            loopStart=loopStartPoints[1];

            // Can't be one channel .mib with interleave values
            if(interleave>0 && channel_count==1)
                channel_count=2;
        } else {
            loopStart=0;
        }
    }

    if(loopEndPointsCount>=2) {
        // can't get more then 0x10 loop point !
        if(loopEndPointsCount<=0x0F) {
            // No need to recalculate interleave value ...
            loopEnd=loopEndPoints[loopEndPointsCount-1];

            // Can't be one channel .mib with interleave values
            if(channel_count==1)
                channel_count=2;
        } else {
            loopToEnd=0;
            loopEnd=0;
        }
    }

    if (loopToEnd)
        loopEnd=fileLength;

    if(forceNoLoop)
        loopEnd=0;

    if(interleave>0x10 && channel_count==1)
        channel_count=2;

    if(interleave==0)
        interleave=0x10;

    // further check on channel_count ...
    if(gotEmptyLine) {
        int newChannelCount = 0;

        readOffset=0;

        /* count empty lines at interleave = channels */
        do {
            newChannelCount++;
            read_streamfile(testBuffer,readOffset,0x10,streamFile);
            readOffset+=interleave;
        } while(!memcmp(testBuffer,mibBuffer,16));

        newChannelCount--;
        if(newChannelCount>channel_count)
            channel_count=newChannelCount;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,(loopEnd!=0));
    if (!vgmstream) goto fail;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;

    vgmstream->interleave_block_size = interleave;

    if(!strcasecmp("mib",filename_extension(filename)))
        vgmstream->sample_rate = 44100;

    if(!strcasecmp("mi4",filename_extension(filename)))
        vgmstream->sample_rate = 48000;

    vgmstream->num_samples = (int32_t)(fileLength/16/channel_count*28);

    if(loopEnd!=0) {
        if(vgmstream->channels==1) {
            vgmstream->loop_start_sample = loopStart/16*18; //todo 18 instead of 28 probably a bug
            vgmstream->loop_end_sample = loopEnd/16*28;
        } else {
            vgmstream->loop_start_sample = ((((loopStart/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*14*channel_count)/channel_count;
            if(loopStart%vgmstream->interleave_block_size) {
                vgmstream->loop_start_sample += (((loopStart%vgmstream->interleave_block_size)-1)/16*14*channel_count);
            }

            if(loopEnd==fileLength) {
                vgmstream->loop_end_sample=(loopEnd/16*28)/channel_count;
            } else {
                vgmstream->loop_end_sample = ((((loopEnd/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*14*channel_count)/channel_count;
                if(loopEnd%vgmstream->interleave_block_size) {
                    vgmstream->loop_end_sample += (((loopEnd%vgmstream->interleave_block_size)-1)/16*14*channel_count);
                }
            }
        }
    }

    if(loopToEnd) {
        // try to find if there's no empty line ...
        int emptySamples=0;

        for(i=0; i<16;i++) {
            mibBuffer[i]=0; //memset
        }

        readOffset=fileLength-0x10;
        do {
            read_streamfile(testBuffer,readOffset,0x10,streamFile);
            if(!memcmp(mibBuffer,testBuffer,16)) {
                emptySamples+=28;
            }
            readOffset-=0x10;
        } while(!memcmp(testBuffer,mibBuffer,16));

        vgmstream->loop_end_sample-=(emptySamples*channel_count);
    }

    vgmstream->meta_type = meta_PS_HEADERLESS;
    vgmstream->allow_dual_stereo = 1;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
