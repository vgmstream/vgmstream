#include "meta.h"
#include "../util.h"
/*
const short wad_coef[16][2] =
{
	{0x4002,0x2003},
	{0x2016,0xc600},
	{0xC600,0x98ab},
	{0x96bf,0x29c5},
	{0x2003,0x0081},
	{0x0e00,0x2004},
	{0x8e01,0xc500},
	{0x70bf,0x8128},
	{0x288e,0xc600},
	{0x016e,0x0e5b},
	{0xbe20,0x2003},
	{0x03c6,0xc600},
	{0x0048,0xe85a},
	{0xbe28,0x28c6},
	{0xc600,0x00F6},
	{0xbeab,0x5520}
};*/
const short wad_coef[16] =
{
	0x04ab, 0xfced,
	0x0789,	0xfedf,
	0x09a2,	0xfae5,
	0x0c90,	0xfac1,
	0x084d,	0xfaa4,
	0x0982,	0xfdf7,
	0x0af6,	0xfafa,
	0x0be6,	0xfbf5
};


/* WAC - WAD - WAM (Beyond Good & Evil GC/PS2/Xbox/Wii) */
/* Note: A "Flat Layout" has no interleave */
VGMSTREAM * init_vgmstream_waa_wac_wad_wam(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
	int i;
    off_t start_offset;
    int loop_flag;
	int channel_count;
	int coef1_start;
	int coef2_start;
	int second_channel_start = -1;

    // Check file extensions
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("waa",filename_extension(filename)) && 
		strcasecmp("wac",filename_extension(filename)) && 
		strcasecmp("wad",filename_extension(filename)) && 
		strcasecmp("wam",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52494646 ||	/* "RIFF" */
		read_32bitBE(0x08,streamFile) != 0x57415645 ||	/* "WAVE" */
		read_32bitBE(0x0C,streamFile) != 0x666D7420 ||	/* "fmt\0x20" */
		read_32bitBE(0x10,streamFile) != 0x12000000) 	/* "0x12000000" */
	goto fail;

	/* files don't contain looping information,
	   so the looping is not done depending on extension.
	   wam and waa contain ambient sounds and music, so often they contain
	   looped music. Change extension to wac or wad to make the sound non-looping.
	*/
    loop_flag = strcasecmp("wac",filename_extension(filename)) && 
		    	strcasecmp("wad",filename_extension(filename));
    channel_count = (uint16_t)read_16bitLE(0x16,streamFile);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* Check what encoder is needed */
	//FIXME: //PC version uses pcm, but which encoder?

	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);
	vgmstream->meta_type = meta_WAA_WAC_WAD_WAM;
    vgmstream->layout_type = layout_none;

    switch((uint16_t)read_16bitLE(0x14,streamFile))	{
    case 0x0069: // XBOX IMA ADPCM
		start_offset = 0x2E;
		vgmstream->coding_type = coding_XBOX;
		vgmstream->num_samples = (read_32bitLE(0x2A,streamFile))/36/channel_count*64;
		if (loop_flag) {
			vgmstream->loop_start_sample = 0;
			vgmstream->loop_end_sample = (read_32bitLE(0x2A,streamFile))/36/channel_count*64;
		}
    break;
    case 0xFFFF: // PS2 ADPCM
		start_offset = 0x2E;
		vgmstream->coding_type = coding_PSX;
		vgmstream->num_samples = (read_32bitLE(0x2A,streamFile))/16*28/channel_count;
		if (loop_flag) {
			vgmstream->loop_start_sample = 0;
			vgmstream->loop_end_sample = (read_32bitLE(0x2A,streamFile))/16*28/channel_count;
		}
            second_channel_start = (read_32bitLE(0x2A,streamFile)/2)+start_offset;
    break;
    case 0xFFFE: // GameCube/WII DSP
		start_offset = 0x5C;
		vgmstream->coding_type = coding_NGC_DSP;
		vgmstream->num_samples = (read_32bitLE(0x2A,streamFile))*14/8/channel_count;
		if (loop_flag) {
			vgmstream->loop_start_sample = 0;
			vgmstream->loop_end_sample = (read_32bitLE(0x2A,streamFile))*14/8/channel_count;
		}
		if(read_16bitLE(0x24,streamFile)==0x00)//is a wii file with no coeff table
		{
			//FIXME: WII version of WAM/WAD/WAC need some coeff table from somewhere
			for (i=0;i<16;i++)
					vgmstream->ch[0].adpcm_coef[i] = wad_coef[i];
			if (channel_count == 2) {
				for (i=0;i<16;i++)
					vgmstream->ch[1].adpcm_coef[i] = wad_coef[i];
			}
			goto fail;
		}
		else
		{
				second_channel_start = (read_32bitLE(0x2A,streamFile)/2)+0x8A;
				/* Retrieveing the coef tables */
				coef1_start = 0x2E;
				coef2_start = (read_32bitLE(0x2A,streamFile)/2)+0x5C;

			{
				int i;
				for (i=0;i<16;i++)
					vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(coef1_start+i*2,streamFile);
				if (channel_count == 2) {
				for (i=0;i<16;i++)
					vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(coef2_start+i*2,streamFile);
				}
			}
		}
        break;
		    default:
			    goto fail;
	}




    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
            
            if (vgmstream->coding_type == coding_XBOX) {
                /* xbox interleaving is a little odd */
                vgmstream->ch[i].channel_start_offset=start_offset;
            } else {
                vgmstream->ch[0].channel_start_offset=start_offset;
            if (channel_count == 2) {
                if (second_channel_start == -1) goto fail;
                vgmstream->ch[1].channel_start_offset=second_channel_start;
            }
        }
        vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;
    }
    }


    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}




