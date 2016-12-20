#include "meta.h"
#include "../util.h"


static int vag_find_loop_points(STREAMFILE *streamFile, off_t * loop_start, off_t * loop_end);

/* PS2 VAG format, found in many Sony games

   2008-05-17 - Fastelbja : First version ...
*/
VGMSTREAM * init_vgmstream_ps2_vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

	off_t loopStart = 0;
	off_t loopEnd = 0;

	uint8_t	vagID;
	off_t start_offset;

	size_t interleave;
	
    int loop_flag=0;
    int channel_count=0;
    int i;
    int loop_samples_found = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("vag",filename_extension(filename))) goto fail;

    /* check VAG Header */
    if (((read_32bitBE(0x00,streamFile) & 0xFFFFFF00) != 0x56414700) && /* "VAG\0" */
 		((read_32bitLE(0x00,streamFile) & 0xFFFFFF00) != 0x56414700))
        goto fail;

	/* Check for correct channel count and loop flag */
	vagID=read_8bit(0x03,streamFile);

	switch(vagID) {
        case '1': /* "VAG1" (1 channel) */
            channel_count=1;
            break;
        case '2': /* "VAG2" (2 channels) */
            channel_count=2;
            break;
		case 'i': /* "VAGi" (interleaved) */
			channel_count=2;
			break;
		case 'V': /* pGAV (little endian header) */
			if(read_32bitBE(0x20,streamFile)==0x53746572) /* "Ster" */
				channel_count=2;
			else
			    channel_count=1;
			break;
		case 'p': /* "VAGp" (extended) */
			if((read_32bitBE(0x04,streamFile)<=0x00000004) && (read_32bitBE(0x0c,streamFile)<(get_streamfile_size(streamFile)/2))) {
				loop_flag=(read_32bitBE(0x14,streamFile)!=0);
				channel_count=2;
			}
			else {
				loop_flag = vag_find_loop_points(streamFile, &loopStart, &loopEnd); /* offset 0x30 */
				channel_count = 1;
			}
			break;
        default:
            goto fail;
	}

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->coding_type = coding_PSX;

    switch(vagID) {
        case '1': // VAG1
			vgmstream->layout_type=layout_none;
			vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
			vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/16*28;
			interleave = read_32bitLE(0x08,streamFile);
            if (interleave != 0) goto fail;
			vgmstream->meta_type=meta_PS2_VAG1;
			start_offset=0x40;
            break;
        case '2': // VAG2
			vgmstream->layout_type=layout_interleave;
			vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
			vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/16*28;
			interleave = 0x800;
			vgmstream->meta_type=meta_PS2_VAG2;
			start_offset=0x40;
            break;
		case 'i': // VAGi
			vgmstream->layout_type=layout_interleave;
			vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
			vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/16*28;
			interleave = read_32bitLE(0x08,streamFile);
			vgmstream->meta_type=meta_PS2_VAGi;
			start_offset=0x800;
			break;
		case 'p': // VAGp
			vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
			interleave=0x10;

			if((read_32bitBE(0x04,streamFile)==0x00000004) && (read_32bitBE(0x0c,streamFile)<(get_streamfile_size(streamFile)/2))) {
				vgmstream->channels=2;
				vgmstream->num_samples = read_32bitBE(0x0C,streamFile);

				if(loop_flag) {
					vgmstream->loop_start_sample=read_32bitBE(0x14,streamFile);
					vgmstream->loop_end_sample =read_32bitBE(0x18,streamFile);
					loop_samples_found = 1;
				}

				start_offset=0x80;
				vgmstream->layout_type=layout_interleave;
				vgmstream->meta_type=meta_PS2_VAGs;

				// Double VAG Header @ 0x0000 & 0x1000
				if(read_32bitBE(0,streamFile)==read_32bitBE(0x1000,streamFile)) {
					vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/16*28;
					interleave=0x1000;
					start_offset=0;
				}
			}
			else {
				vgmstream->layout_type=layout_none;
				vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/16*28;
				vgmstream->meta_type=meta_PS2_VAGp;
				start_offset=0x30;
			}
			break;
		case 'V': // pGAV
			vgmstream->layout_type=layout_interleave;
			interleave=0x2000;

			// Jak X hack ...
			if(read_32bitLE(0x1000,streamFile)==0x56414770)
				interleave=0x1000;

			vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
			vgmstream->num_samples = read_32bitLE(0x0C,streamFile)/16*14;
			vgmstream->meta_type=meta_PS2_pGAV;
			start_offset=0;
			break;
        default:
            goto fail;
	}

	vgmstream->interleave_block_size=interleave;
	
	/* Don't add the header size to loop calc points */
	if(loop_flag && !loop_samples_found) {
		loopStart-=start_offset;
		loopEnd-=start_offset;

        vgmstream->loop_start_sample = (int32_t)((loopStart/(interleave*channel_count))*interleave)/16*28;
        vgmstream->loop_start_sample += (int32_t)(loopStart%(interleave*channel_count))/16*28;
        vgmstream->loop_end_sample = (int32_t)((loopEnd/(interleave*channel_count))*interleave)/16*28;
        vgmstream->loop_end_sample += (int32_t)(loopEnd%(interleave*channel_count))/16*28;
	}


    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            if (vgmstream->interleave_block_size > 0) {
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,vgmstream->interleave_block_size);
            } else {
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            }

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                (off_t)(start_offset+vgmstream->interleave_block_size*i);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/**
 * Finds loop points in VAG data using flag markers and updates loop_start and loop_end with the offsets.
 *
 * returns 0 if not found
 */
static int vag_find_loop_points(STREAMFILE *streamFile, off_t * loop_start, off_t * loop_end) {
    off_t loopStart = 0;
    off_t loopEnd = 0;

    /* used for loop points ... */
    uint8_t eofVAG[16]={0x00,0x07,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77};
    uint8_t eofVAG2[16]={0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t readbuf[16];

    /* Search for loop in VAG */
    size_t fileLength = get_streamfile_size(streamFile);


    off_t readOffset = 0x20;
    do {
        readOffset+=0x10;

        // Loop Start ...
        if(read_8bit(readOffset+0x01,streamFile)==0x06) {
            if(loopStart==0) loopStart = readOffset;
        }

        // Loop End ...
        if(read_8bit(readOffset+0x01,streamFile)==0x03) {
            if(loopEnd==0) loopEnd = readOffset;
        }

        // Loop from end to beginning ...
        if((read_8bit(readOffset+0x01,streamFile)==0x01)) {
            // Check if we have the eof tag after the loop point ...
            // if so we don't loop, if not present, we loop from end to start ...
            read_streamfile(readbuf,readOffset+0x10,0x10,streamFile);
            if((readbuf[0]!=0) && (readbuf[0]!=0x0c)) {
                if(memcmp(readbuf,eofVAG,0x10) && (memcmp(readbuf,eofVAG2,0x10))) {
                    loopStart = 0x40;
                    loopEnd = readOffset;
                }
            }
        }

    } while (streamFile->get_offset(streamFile)<(off_t)fileLength);


    if (loopEnd) {
        *loop_start = loopStart;
        *loop_end = loopEnd;
        return 1;
    }

    return 0;
}
