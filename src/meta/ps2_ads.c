#include "meta.h"
#include "../util.h"

/* .ADS - Sony's "Audio Stream" format [Edit Racing (PS2), Evergrace II (PS2), Pri-Saga! Portable (PSP)] */
VGMSTREAM * init_vgmstream_ps2_ads(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    off_t start_offset;
    size_t stream_size;
    uint32_t loop_start, loop_end;


    /* checks */
    /* .ads: actual extension
     * .ss2: demuxed videos (fake?)
     * .pcm: Taisho Mononoke Ibunroku (PS2)
     * .adx: Armored Core 3 (PS2) */
    if (!check_extensions(streamFile, "ads,ss2,pcm,adx"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x53536864 &&  /* "SShd" */
        read_32bitBE(0x20,streamFile) != 0x53536264)    /* "SSbd" */
        goto fail;
    /* 0x04: header size, always 0x20 */


    /* check if file is not corrupt */ //todo ???
    /* seems the Gran Turismo 4 ADS files are considered corrupt,*/
    /* so I changed it to adapt the stream size if that's the case */
    /* instead of failing playing them at all*/
    stream_size = read_32bitLE(0x24,streamFile); /* body size */
    if (stream_size + 0x28 >= get_streamfile_size(streamFile)) {
        stream_size = get_streamfile_size(streamFile) - 0x28;
    }

    /* check loop */
    loop_start = read_32bitLE(0x18,streamFile);
    loop_end = read_32bitLE(0x1C,streamFile);

    //todo should loop if loop_start > 0 and loop_end == -1
    if ((loop_end == 0xFFFFFFFF) || (loop_start == 0 && loop_end == 0)) {
        loop_flag = 0;
    }
    else {
        loop_flag = 1;
    }

    channel_count = read_32bitLE(0x10,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);

    //todo use proper flags
    if (read_32bitLE(0x08,streamFile)!=0x10) {
        vgmstream->coding_type = coding_PCM16LE;
        vgmstream->num_samples = stream_size/2/vgmstream->channels;
    } else {
        vgmstream->coding_type = coding_PSX;
        vgmstream->num_samples = ((stream_size-0x40)/16*28)/vgmstream->channels; //todo don't - 0x40?
    }

    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile); /* set even in mono */
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_SShd;

    /* loops */
    if (vgmstream->loop_flag) {
        if ((loop_end*0x10*vgmstream->channels+0x800) == get_streamfile_size(streamFile)) {
            /* full loop? */ //todo needed???
            uint8_t testBuffer[0x10];
            off_t readOffset = 0, loopEndOffset = 0;

            readOffset = (off_t)get_streamfile_size(streamFile)-(4*vgmstream->interleave_block_size);
            do {
                readOffset += (off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile);

                if(testBuffer[0x01]==0x01) {
                    if(loopEndOffset==0)
                        loopEndOffset = readOffset-0x10;
                    break;
                }

            } while (streamFile->get_offset(streamFile)<(int32_t)get_streamfile_size(streamFile));

            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = (loopEndOffset/(vgmstream->interleave_block_size)*vgmstream->interleave_block_size)/16*28;
            vgmstream->loop_end_sample += (loopEndOffset%vgmstream->interleave_block_size)/16*28;
            vgmstream->loop_end_sample /=vgmstream->channels;
        }
        else {
            if (loop_end <= vgmstream->num_samples) {
                /* assume loops are samples */
                vgmstream->loop_start_sample = loop_start;
                vgmstream->loop_end_sample = loop_end;
            } else {
                /* assume loops are addresses (official definition) */ //todo use interleave instead of 0x10?
                vgmstream->loop_start_sample = (loop_start*0x10)/16*28/vgmstream->channels;
                vgmstream->loop_end_sample = (loop_end*0x10)/16*28/vgmstream->channels;
            }
        }

        /* don't know why, but it does happen, in ps2 too :( */ //todo what
        if (vgmstream->loop_end_sample > vgmstream->num_samples)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }


    /* adjust */
    start_offset = 0x28;

    if ((stream_size * 2) == (get_streamfile_size(streamFile) - 0x18)) {
        /* True Fortune (PS2) with weird stream size */ //todo try to move
        stream_size = (read_32bitLE(0x24,streamFile) * 2) - 0x10;
        vgmstream->num_samples = stream_size / 16 * 28 / vgmstream->channels;
    }
    else if(get_streamfile_size(streamFile) - read_32bitLE(0x24,streamFile) >= 0x800) {
        /* Hack for files with start_offset = 0x800 (ex. Taisho Mononoke Ibunroku) */
        start_offset = 0x800;
    }

    if (vgmstream->coding_type == coding_PSX && start_offset == 0x28) {
        int i;
        start_offset = 0x800;

        for (i=0; i < 0x1f6; i += 4) {
            if (read_32bitLE(0x28+(i*4),streamFile)!=0) {
                start_offset = 0x28;
                break;
            }
        }
    }

    //todo should adjust num samples after changing start_offset and stream_size?


    /* check if we got a real pcm by checking PS-ADPCM flags (ex: Clock Tower 3) */
    //todo check format 0x02 instead
    if (vgmstream->coding_type==coding_PCM16LE) {
        uint8_t isPCM = 0;
        off_t check_offset;

        check_offset = start_offset;
        do {
            if (read_8bit(check_offset+1,streamFile)>7) {
                isPCM=1;
                break;
            }
            else {
                check_offset+=0x10;
            }

        } while (check_offset<get_streamfile_size(streamFile));

        if (!isPCM) {
            vgmstream->num_samples=(get_streamfile_size(streamFile)-start_offset)/16*28/vgmstream->channels;
            vgmstream->coding_type=coding_PSX;
        }
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
