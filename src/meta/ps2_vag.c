#include "meta.h"


static int vag_find_loop_offsets(STREAMFILE *streamFile, off_t start_offset, off_t * loop_start, off_t * loop_end);

/* VAGp - SDK format, created by various Sony's tools (like AIFF2VAG) */
VGMSTREAM * init_vgmstream_ps2_vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, loopStart = 0, loopEnd = 0;

    uint8_t vagID;
    uint32_t version = 0;

    size_t filesize = 0, datasize = 0, interleave;

    int loop_flag = 0, loop_samples_found = 0;
    int channel_count = 0;
    int is_swag = 0;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"vag,swag,str") )
        goto fail;

    /* check VAG Header */
    if (((read_32bitBE(0x00,streamFile) & 0xFFFFFF00) != 0x56414700) && /* "VAG" */
        ((read_32bitLE(0x00,streamFile) & 0xFFFFFF00) != 0x56414700))
        goto fail;

    /* Frantix VAGp .swag: some (not all) fields in LE + 2 VAGp in the same file (full interleave) */
    is_swag = check_extensions(streamFile,"swag");

    filesize = get_streamfile_size(streamFile);

    /* version used to create the file
     *  ex: 00000000 = v1.8 PC, 00000002 = v1.3 Mac, 00000003 = v1.6+ Mac, 00000020 = v2.0+ PC */
    version = read_32bitBE(0x04,streamFile);
    /* 0x08-0c: reserved */
    if (is_swag)
        datasize = read_32bitLE(0x0c,streamFile);
    else
        datasize = read_32bitBE(0x0c,streamFile);
    /* 0x14-20 reserved */
    /* 0x20-30: name (optional) */
    /* 0x30: data start (first 0x10 usually 0s to init SPU) */

    /* Check for correct channel count and loop flag */
    vagID=read_8bit(0x03,streamFile);
    switch(vagID) {
        case '1': /* "VAG1" (1 channel) [Metal Gear Solid 3] */
            channel_count=1;
            break;
        case '2': /* "VAG2" (2 channels) [Metal Gear Solid 3] */
            channel_count=2;
            break;
        case 'i': /* "VAGi" (interleaved) */
            channel_count=2;
            break;
        case 'V': /* pGAV (little endian / stereo) [Jak 3, Jak X] */
            if (read_32bitBE(0x20,streamFile)==0x53746572) /* "Ster" */
                channel_count=2;
            else
                channel_count=1;
            break;
        case 'p': /* "VAGp" (extended) [most common, ex Ratchet & Clank] */

            if ((version <= 0x00000004) && (datasize < filesize / 2)) { /* two VAGp in the same file */
                if (is_swag)
                    loop_flag = vag_find_loop_offsets(streamFile, 0x30, &loopStart, &loopEnd);
                else
                    loop_flag = read_32bitBE(0x14,streamFile) != 0;
                channel_count=2;
            }
            else if (version == 0x00020001) { /* HEVAG */
                loop_flag = vag_find_loop_offsets(streamFile, 0x30, &loopStart, &loopEnd);

                /* channels are usually at 0x1e, but not in Ukiyo no Roushi which has some kind
                 *  of loop-like values instead (who designs this crap?) */
                if (read_32bitBE(0x18,streamFile) != 0 || read_32bitBE(0x1c,streamFile) > 0x20) {
                    channel_count = 1;
                } else {
                    channel_count = read_8bit(0x1e,streamFile);
                    if (channel_count == 0)
                        channel_count = 1;  /* ex. early Vita vag (Lumines) */
                }
            }
            else {
                loop_flag = vag_find_loop_offsets(streamFile, 0x30, &loopStart, &loopEnd);
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
    vgmstream->coding_type = coding_PSX;
    if (is_swag)
        vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    else
        vgmstream->sample_rate = read_32bitBE(0x10,streamFile);

    switch(vagID) {
        case '1': // VAG1
            vgmstream->layout_type=layout_none;
            vgmstream->num_samples = datasize / 16 * 28;
            interleave = read_32bitLE(0x08,streamFile);
            if (interleave != 0) goto fail;
            vgmstream->meta_type=meta_PS2_VAG1;
            start_offset=0x40; /* 0x30 is extra data in VAG1 */
            break;
        case '2': // VAG2
            vgmstream->layout_type=layout_interleave;
            vgmstream->num_samples = datasize / 16 * 28; /* datasize is for 1 channel only in VAG2 */
            interleave = 0x800;
            vgmstream->meta_type=meta_PS2_VAG2;
            start_offset=0x40; /* 0x30 is extra data in VAG2 */
            break;
        case 'i': // VAGi
            vgmstream->layout_type=layout_interleave;
            vgmstream->num_samples = datasize / 16 * 28;
            interleave = read_32bitLE(0x08,streamFile);
            vgmstream->meta_type=meta_PS2_VAGi;
            start_offset=0x800;
            break;
        case 'p': // VAGp
            interleave=0x10;

            if ((version == 0x00000004) && (datasize < filesize / 2)) {
                vgmstream->channels=2;
                vgmstream->layout_type=layout_interleave;
                vgmstream->meta_type=meta_PS2_VAGs;

                if (is_swag) {
                    start_offset = 0x30;
                    interleave = datasize;
                    vgmstream->num_samples = datasize / 16 * 28;
                    vgmstream->loop_start_sample = (loopStart-start_offset) / 16 * 28;
                    vgmstream->loop_end_sample = (loopEnd-start_offset) / 16 * 28;
                    loop_samples_found = 1;

                } else {
                    start_offset=0x80;
                    vgmstream->num_samples = datasize; /* todo test if datasize/16*28? */
                    if(loop_flag) {
                        vgmstream->loop_start_sample=read_32bitBE(0x14,streamFile);
                        vgmstream->loop_end_sample =read_32bitBE(0x18,streamFile);
                        loop_samples_found = 1;
                        // Double VAG Header @ 0x0000 & 0x1000
                        if(read_32bitBE(0,streamFile)==read_32bitBE(0x1000,streamFile)) {
                            vgmstream->num_samples = datasize / 16 * 28;
                            interleave=0x1000;
                            start_offset=0;
                        }
                    }
                }
            }
            else if (version == 0x40000000) { /* Guerilla VAG (little endian) */
                datasize = read_32bitLE(0x0c,streamFile);
                vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
                vgmstream->layout_type=layout_none;
                vgmstream->meta_type=meta_PS2_VAGp;

                vgmstream->num_samples = datasize / channel_count / 16 * 28;
                start_offset = 0x30;
            }
            else if (version == 0x00020001) { /* HEVAG */
                vgmstream->coding_type = coding_HEVAG;
                vgmstream->layout_type = layout_interleave;
                vgmstream->meta_type   = meta_PS2_VAGs;

                vgmstream->num_samples = datasize / channel_count / 16 * 28;
                start_offset = 0x30;
            }
            else { /* VAGp, usually separate L/R files */
                vgmstream->layout_type=layout_none;
                vgmstream->meta_type=meta_PS2_VAGp;

                vgmstream->num_samples = datasize / channel_count / 16 * 28;
                start_offset=0x30;
            }
            break;
		case 'V': // pGAV
			vgmstream->layout_type=layout_interleave;
			interleave=0x2000; /* Jak 3 interleave, includes header */

			if(read_32bitLE(0x1000,streamFile)==0x56414770) /* "pGAV" */
				interleave=0x1000; /* Jak X interleave, includes header */

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


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/**
 * Finds loop points in VAG data using flag markers and updates loop_start and loop_end with the global offsets.
 *
 * returns 0 if not found
 */
static int vag_find_loop_offsets(STREAMFILE *streamFile, off_t start_offset, off_t * loop_start, off_t * loop_end) {
    off_t loopStart = 0;
    off_t loopEnd = 0;

    /* used for loop points (todo: variations: 0x0c0700..00, 0x070077..77  ) */
    /* 'used to prevent unnecessary SPU interrupts' (optional if no IRQ or no looping) */
    uint8_t eofVAG[16]={0x00,0x07,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77};
    uint8_t eofVAG2[16]={0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t readbuf[16];
    uint8_t flag;

    /* Search for loop in VAG */
    size_t fileLength = get_streamfile_size(streamFile);


    off_t readOffset = start_offset - 0x10;
    do {
        readOffset+=0x10;

        flag = read_8bit(readOffset+0x01,streamFile) & 0x0F; /* lower nibble (for HEVAG) */

        // Loop Start ...
        if (flag == 0x06 && !loopStart) {
            loopStart = readOffset;
        }

        // Loop End ...
        if (flag == 0x03 && !loopEnd) {
            loopEnd = readOffset;

            if (loopStart && loopEnd)
               break;
        }

        /* hack for some games that don't have loop points but play the same track on repeat
         * (sometimes this will loop non-looping tracks incorrectly)
         * if there is a "partial" 0x07 end flag pretend it wants to loop */
        if (flag == 0x01) {
            // Check if we have a full eof tag after the loop point ...
            // if so we don't loop, if not present, we loop from end to start ...
            int read = read_streamfile(readbuf,readOffset+0x10,0x10,streamFile);
            /* is there valid data after flag 0x1? */
            if (read > 0
                    && readbuf[0] != 0x00
                    && readbuf[0] != 0x0c
                    && readbuf[0] != 0x3c /* Ecco the Dolphin, Ratchet & Clank 2  */
                    ) { 
                if (memcmp(readbuf,eofVAG,0x10) && (memcmp(readbuf,eofVAG2,0x10))) { /* full end flags */
                    loopStart = start_offset + 0x10; /* todo proper start */
                    loopEnd = readOffset;
                    break;
                }
            }
        }

    } while (streamFile->get_offset(streamFile)<(off_t)fileLength);


    if (loopStart && loopEnd) {
        *loop_start = loopStart;
        *loop_end = loopEnd;
        return 1;
    }

    return 0;
}
