//MxSt files ripped from Jukebox.si in Lego Island
#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_pc_mxst(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    int loop_flag=0;
	int bits_per_sample;
	int channel_count;
    int sample_rate,bytes_per_second;
    long sample_count;
    int i;
    off_t file_size;
    off_t chunk_list_size=-1;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mxst",filename_extension(filename))) goto fail;

    /* looping info not found yet */
	//loop_flag = get_streamfile_size(streamFile) > 700000;

    /* check MxSt header */
    if (0x4d785374 != read_32bitBE(0, streamFile)) goto fail;
    file_size = read_32bitLE(4, streamFile) + 8;
    if (file_size != get_streamfile_size(streamFile)) goto fail;

    /* read chunks */
    {
        off_t MxDa=-1; /* points inside the MxDa chunk */
        off_t MxCh=-1; /* point at start of the first MxCh chunk */
        off_t chunk_offset = 8;
        uint32_t stream_id;
        while (chunk_offset < file_size)
        {
            uint32_t chunk_size = (read_32bitLE(chunk_offset+4, streamFile)+1)/2*2;
            switch (read_32bitBE(chunk_offset, streamFile))
            {
                case 0x4d784f62:    /* MxOb */
                    /* not interesting for playback */
                    break;
                case 0x20574156:    /* " WAV" */
                    if (chunk_size == 1)
                        chunk_size = 8;
                    break;
                case 0x4c495354:    /* LIST */
                {
                    off_t first_item_offset = chunk_offset+0x14;
                    off_t list_chunk_offset = first_item_offset+
                        read_32bitLE(chunk_offset+0x10,streamFile);

                    if (read_32bitBE(chunk_offset+0x8,streamFile) == 0x4d784461) /* MxDa */
                        MxDa = first_item_offset;
                    else
                        goto fail;

                    if (read_32bitBE(chunk_offset+0xC,streamFile) ==
                        0x4d784368) /* MxCh */
                    {
                        MxCh = list_chunk_offset;
                        chunk_list_size = chunk_size - (list_chunk_offset-(chunk_offset+8));
                    }
                    else
                        goto fail;

                    break;
                }
                default:
                    goto fail;
            }

            chunk_offset += 8 + chunk_size;
            if (chunk_offset > file_size) goto fail;
        }

        if (MxDa == -1 || MxCh == -1 || chunk_list_size == -1) goto fail;
        
        /* parse MxDa */
        {
            /* ??? */
            if (0 != read_16bitLE(MxDa+0x00,streamFile)) goto fail;
            stream_id = read_32bitLE(MxDa+0x2,streamFile);
            /* First sample (none in MxDa block) */
            if (-1 != read_32bitLE(MxDa+0x06,streamFile)) goto fail;
            /* size of format data */
            if (0x18 != read_32bitLE(MxDa+0x0a,streamFile)) goto fail;
            /* PCM */
            if (1 != read_16bitLE(MxDa+0x0e,streamFile)) goto fail;
            /* channel count */
            channel_count = read_16bitLE(MxDa+0x10,streamFile);
            /* only mono known */
            if (1 != channel_count) goto fail;
            sample_rate = read_32bitLE(MxDa+0x12,streamFile);
            bits_per_sample = read_16bitLE(MxDa+0x1c,streamFile);
            /* bytes per second */
            bytes_per_second = read_32bitLE(MxDa+0x16,streamFile);
            if (bits_per_sample/8*channel_count*sample_rate != bytes_per_second) goto fail;
            /* block align */
            if (bits_per_sample/8*channel_count !=
                read_16bitLE(MxDa+0x1a,streamFile)) goto fail;
            sample_count = read_32bitLE(MxDa+0x1e,streamFile)/(bits_per_sample/8)/channel_count;
            /* 2c? data offset in normal RIFF WAVE? */
            if (0x2c != read_32bitLE(MxDa+0x22,streamFile)) goto fail;
        }

        /* parse through all MxCh for consistency check */
        {
            long samples = 0;
            int split_frames_seen = 0;
            off_t MxCh_offset = MxCh;
            while (MxCh_offset < MxCh+chunk_list_size)
            {
                uint16_t flags;
                if (read_32bitBE(MxCh_offset,streamFile)!=0x70616420) /*pad*/
                {
                    if (read_32bitBE(MxCh_offset,streamFile)!=0x4d784368) /*MxCh*/
                        goto fail;

                    flags = read_16bitLE(MxCh_offset+8+0,streamFile);

                    if (read_32bitLE(MxCh_offset+8+2,streamFile)!=stream_id)
                        goto fail;

                    if (flags & 0x10)
                    {
                        split_frames_seen ++;
                        if (split_frames_seen == 1)
                        {
                            if (read_32bitLE(MxCh_offset+8+6,streamFile)!=(samples*UINT64_C(1000)+sample_rate-1)/sample_rate)
                                goto fail;
                        }
                        else if (split_frames_seen == 2)
                        {
                            if ( read_32bitLE(MxCh_offset+8+0xa,streamFile)!=
                                    read_32bitLE(MxCh_offset+4,streamFile)-0xe ) goto fail;
                            split_frames_seen = 0;
                        }
                        else goto fail;
                    }

                    if (!(flags & 0x10))
                    {
                        if (split_frames_seen != 0)
                        {
                            goto fail;
                        }
                        if (read_32bitLE(MxCh_offset+8+6,streamFile)!=(samples*UINT64_C(1000)+sample_rate-1)/sample_rate)
                            goto fail;

                        if ( read_32bitLE(MxCh_offset+8+0xa,streamFile)!=
                             read_32bitLE(MxCh_offset+4,streamFile)-0xe ) goto fail;
                    }
                    samples += (read_32bitLE(MxCh_offset+4,streamFile)-0xe)/(bits_per_sample/8/channel_count);

                }
                MxCh_offset += 8 + (read_32bitLE(MxCh_offset+4,streamFile)+1)/2*2;
                if (MxCh_offset > MxCh+chunk_list_size) goto fail;
            }
            //printf("samples=%d sample_count=%d\n",samples,sample_count);
            //samples = (samples * (bits_per_sample/8) * channel_count + 31)/32*32/(bits_per_sample/8)/channel_count;
            if (samples < sample_count)
            {
                sample_count = samples;
            }
            if (samples != sample_count) goto fail;
        }

        start_offset = MxCh;
    }
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->layout_type = layout_blocked_mxch;
	
    vgmstream->meta_type = meta_PC_MXST;
	if(bits_per_sample == 8)
	{
		vgmstream->coding_type = coding_PCM8_U;
	}
	else if (bits_per_sample == 16)
	{
		vgmstream->coding_type = coding_PCM16LE;
	}
    else goto fail;
	vgmstream->num_samples = sample_count;
	if(loop_flag)
	{
		vgmstream->loop_start_sample = 0;
		vgmstream->loop_end_sample=vgmstream->num_samples;
	}
    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile =
                streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;
        }

    }
    block_update_mxch(start_offset, vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
