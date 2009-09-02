#include "meta.h"
#include "../util.h"

/* SOB/SAB combination as found in Worms 4: Mayhem
they are actually soundpacks, but the audio data is just streamed as one big stream
*/

VGMSTREAM * init_vgmstream_sab(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
	STREAMFILE* sob = NULL;
    char filename[260];
	int i;
	int loop_flag, channel_count, numSounds;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sab",filename_extension(filename))) goto fail;

	/* read information from sob file*/
	filename[strlen(filename)-2]='o'; //change extension in sob of file
	sob = open_stdio_streamfile(filename);
	if(!sob) goto fail;
	filename[strlen(filename)-2]='a';//change back to original file

	if (read_32bitBE(0,streamFile)!=0x43535732)//CSW2 header sab file
	{
		goto fail;
	}
	if (read_32bitBE(0,sob)!=0x43544632)//CTF2 header sob file
	{
		goto fail;
	}
	numSounds = read_32bitLE(8,sob);
	if(numSounds==1)
	{//it means it's a single stream and not a voice bank
		loop_flag = 1;
	}else
	{
		loop_flag = 0;
	}
    
	/* Read channels */
	channel_count = read_32bitLE(0x30,sob);
	if( (channel_count>2)||(numSounds>1))/* dirty hack for number of channels*/
		channel_count = 1;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	//is offset OK. sab files can contain more audio files, but without the sob it's just a long stream of sound effects
	vgmstream->current_block_offset=8+32*numSounds;
    vgmstream->channels = channel_count;
	vgmstream->sample_rate = read_32bitLE(0x20,streamFile);
    vgmstream->coding_type = coding_PCM16LE_int;
    vgmstream->num_samples = (int32_t)((get_streamfile_size(streamFile)-vgmstream->current_block_offset)/2/channel_count);
	if(loop_flag)
	{
		vgmstream->loop_start_sample = 0;
		vgmstream->loop_end_sample = vgmstream->num_samples;
	}
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_PC_SOB_SAB;
	
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!vgmstream->ch[i].streamfile) goto fail;
            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=vgmstream->current_block_offset+2*i;
        }
    }
	close_streamfile(sob);
    return vgmstream;

    /* clean up anything we may have opened */
fail:
	if (sob) close_streamfile(sob);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
