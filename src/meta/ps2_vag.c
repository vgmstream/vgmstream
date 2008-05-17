#include "meta.h"
#include "../util.h"

/* VAG

   PS2 SVAG format is an interleaved format found in many SONY Games                
   The header start with a "VAG" id and is follow by :

		i : interleaved format 

   2008-05-17 - Fastelbja : First version ...
*/

VGMSTREAM * init_vgmstream_ps2_vag(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;
	
	// used for loop points ...
	uint8_t eofVAG[16]={0x00,0x07,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77};
	uint8_t eofVAG2[16]={0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	uint8_t readbuf[16];

	off_t readOffset = 0x20;

	off_t loopStart = 0;
	off_t loopEnd = 0;

	uint8_t	vagID;
	off_t start_offset;
	size_t fileLength;

	size_t interleave;
	
    int loop_flag=0;
    int channel_count;
    int i;

    /* check extension, case insensitive */
    if (strcasecmp("vag",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile_buffer(filename,0x8000);
    if (!infile) goto fail;

    /* check VAG Header */
    if (((read_32bitBE(0x00,infile) & 0xFFFFFF00) != 0x56414700) && 
 		((read_32bitLE(0x00,infile) & 0xFFFFFF00) != 0x56414700))
        goto fail;

	/* Check for correct channel count */
	vagID=read_8bit(0x03,infile);

	switch(vagID) {
		case 'i':
			channel_count=2;
			break;
		case 'V':
			if(read_32bitBE(0x20,infile)==0x53746572) // vag Stereo
				channel_count=2;
			break;
		case 'p':
			channel_count=1;

			/* Search for loop in VAG */
			fileLength = get_streamfile_size(infile);

			do {
				readOffset+=0x10; 
				
				// Loop Start ...
				if(read_8bit(readOffset+0x01,infile)==0x06) {
					if(loopStart==0) loopStart = readOffset;
				}

				// Loop End ...
				if(read_8bit(readOffset+0x01,infile)==0x03) {
					if(loopEnd==0) loopEnd = readOffset;
				}

				// Loop from end to beginning ...
				if((read_8bit(readOffset+0x01,infile)==0x01)) {
					// Check if we have the eof tag after the loop point ...
					// if so we don't loop, if not present, we loop from end to start ...
					read_streamfile(readbuf,readOffset+0x10,0x10,infile);
					if((readbuf[0]!=0) && (readbuf[0]!=0x0c)) {
						if(memcmp(readbuf,eofVAG,0x10) && (memcmp(readbuf,eofVAG2,0x10))) {
							loopStart = 0x40;
							loopEnd = readOffset;
						}
					}
				}

			} while (infile->offset<(off_t)fileLength);
			loop_flag = (loopEnd!=0);
			break;
	}

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;

	switch(vagID) {
		case 'i': // VAGi
			vgmstream->layout_type=layout_interleave;
			vgmstream->sample_rate = read_32bitBE(0x10,infile);
			vgmstream->num_samples = read_32bitBE(0x0C,infile)/16*28;
			interleave = read_32bitLE(0x08,infile);
			vgmstream->meta_type=meta_PS2_VAGi;
			start_offset=0x800;
			break;
		case 'p': // VAGp
			vgmstream->layout_type=layout_none;
			vgmstream->sample_rate = read_32bitBE(0x10,infile);
			vgmstream->num_samples = read_32bitBE(0x0C,infile)/16*28;
			vgmstream->meta_type=meta_PS2_VAGp;
			interleave=0x10; // used for loop calc
			start_offset=0x30;
			break;
		case 'V': // pGAV
			vgmstream->layout_type=layout_interleave;
			interleave=0x2000;

			// Jak X hack ...
			if(read_32bitLE(0x1000,infile)==0x56414770)
				interleave=0x1000;

			vgmstream->sample_rate = read_32bitLE(0x10,infile);
			vgmstream->num_samples = read_32bitLE(0x0C,infile)/16*14;
			vgmstream->meta_type=meta_PS2_pGAV;
			start_offset=0;
			break;
	}

	vgmstream->interleave_block_size=interleave;
	
	/* Don't add the header size to loop calc points */
	loopStart-=start_offset;
	loopEnd-=start_offset;

	if(loop_flag!=0) {
		vgmstream->loop_start_sample = (int32_t)((loopStart/(interleave*channel_count))*interleave)/16*28;
		vgmstream->loop_start_sample += (int32_t)(loopStart%(interleave*channel_count))/16*28;
		vgmstream->loop_end_sample = (int32_t)((loopEnd/(interleave*channel_count))*interleave)/16*28;
		vgmstream->loop_end_sample += (int32_t)(loopEnd%(interleave*channel_count))/16*28;
	}

    /* Compression Scheme */
    vgmstream->coding_type = coding_PSX;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,vgmstream->interleave_block_size);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                (off_t)(start_offset+vgmstream->interleave_block_size*i);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
