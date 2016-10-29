#include "meta.h"
#include "../util.h"

/* 2PFS (Konami)
    - Mahoromatic: Moetto - KiraKira Maid-San (PS2) [.2pfs (V1, 2003)]
    - GANTZ The Game (PS2) [.sap (V2, 2005)]

    There are two versions of the format, though they use different extensions.
    Implemented both versions here in case there are .2pfs with the V2 header out there.
    Both loop correctly AFAIK (there is a truncated Mahoromatic rip around, beware).
*/
VGMSTREAM * init_vgmstream_ps2_2pfs(STREAMFILE *streamFile) 
{
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    
    off_t start_offset = 0x800;
    int interleave = 0x1000;

	int loop_flag;
	int channel_count;
	int version; /* v1=1, v2=2 */

    int loop_start_block; /* block number where the loop starts */
    int loop_end_block; /* usually the last block */
    int loop_start_sample_adjust; /* loops start/end a few samples into the start/end block */
    int loop_end_sample_adjust;


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if ( strcasecmp("2pfs",filename_extension(filename))
         && strcasecmp("sap",filename_extension(filename))  )
        goto fail;

    /* check header ("2PFS") */
    if (read_32bitBE(0x00,streamFile) != 0x32504653)
        goto fail;

    version = read_16bitLE(0x04,streamFile);
    if ( version!=0x01 && version!=0x02 )
        goto fail;


    channel_count = read_8bit(0x40,streamFile);
    loop_flag = read_8bit(0x41,streamFile);
    /* other header values
     *  0x06 (4): unknown, v1=0x0004 v2=0x0001
     *  0x08 (32): unique file id
     *  0x0c (32): base header size (v1=0x50, v2=0x60) + datasize (without the 0x800 full header size)
     *  0x10-0x30: unknown (v1 differs from v2)
     *  0x38-0x40: unknown (v1 same as v2)
     *  0x4c (32) in V2: unknown, some kind of total samples?
     */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitLE(0x34,streamFile) * 28 / 16 / channel_count;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_PS2_2PFS;

    if ( version==0x01 ) {
        vgmstream->sample_rate = read_32bitLE(0x44,streamFile);
        loop_start_sample_adjust = read_16bitLE(0x42,streamFile);
        loop_start_block = read_32bitLE(0x48,streamFile);
        loop_end_block = read_32bitLE(0x4c,streamFile);
    } else {
        vgmstream->sample_rate = read_32bitLE(0x48,streamFile);
        loop_start_sample_adjust = read_32bitLE(0x44,streamFile);
        loop_start_block = read_32bitLE(0x50,streamFile);
        loop_end_block = read_32bitLE(0x54,streamFile);
    }
    loop_end_sample_adjust = interleave; /* loops end after all samples in the end_block AFAIK */

    if ( loop_flag ) {
        /* block to offset > offset to sample + adjust (number of samples into the block) */
        vgmstream->loop_start_sample = ((loop_start_block * channel_count * interleave)
                * 28 / 16 / channel_count)
                + (loop_start_sample_adjust * 28 / 16);
        vgmstream->loop_end_sample = ((loop_end_block * channel_count * interleave)
                * 28 / 16 / channel_count)
                + (loop_end_sample_adjust * 28 / 16);
    }



    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        
		for (i=0;i<channel_count;i++) 
		{
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset =
                vgmstream->ch[i].offset =
                        start_offset + (vgmstream->interleave_block_size * i);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
