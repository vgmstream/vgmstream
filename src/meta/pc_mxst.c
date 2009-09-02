//MxSt files ripped from Jukebox.si in Lego Island
#include "meta.h"
#include "../util.h"

//counts the number of audio data bytes.
//any way to speed this up?
static size_t getNumBytes(off_t offset,STREAMFILE* streamFile) {
	off_t j;
	size_t siz = 0;
	for(j=offset;j<get_streamfile_size(streamFile);j+=2)
	{
		if(read_32bitBE(j,streamFile)==0x4d784368)//look for MxCh
		{
			if(read_32bitBE(j+4,streamFile)!=0x4d784368)
				siz += read_32bitLE( j + 18, streamFile);
		}
	}
	return siz;
  }
VGMSTREAM * init_vgmstream_pc_mxst(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    int loop_flag=0;
	int bitrate=2;
	int channel_count;
    int i;
	off_t j;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mxst",filename_extension(filename))) goto fail;

    /* looping info not found yet */
	loop_flag = get_streamfile_size(streamFile) > 700000;
    
	/* Always mono files */
	channel_count=1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = 1;
    vgmstream->sample_rate = 11025;
    vgmstream->layout_type = layout_mxch_blocked;
	
    vgmstream->meta_type = meta_PC_MXST;
	for(j=0;j<get_streamfile_size(streamFile);j++)
	{
		if(read_32bitBE(j,streamFile)==0x4D784461)//look for MxDa
			break;
	}
	if(8==read_8bit(40+j,streamFile))
	{
		vgmstream->coding_type = coding_PCM8_U;
		bitrate=1;
	}
	else
	{
		vgmstream->coding_type = coding_PCM16LE;
	}
	if(j==get_streamfile_size(streamFile))
		goto fail;
	j+=4;
	vgmstream->current_block_offset=0;
	vgmstream->next_block_offset=j;
	vgmstream->num_samples = getNumBytes(j,streamFile) / bitrate / vgmstream->channels;
	if(loop_flag)
	{
		vgmstream->loop_start_sample = 0;
		vgmstream->loop_end_sample=vgmstream->num_samples;
	}
    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
			
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,j);

            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
