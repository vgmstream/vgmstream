#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_ps2_mtaf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag=0;
    int channel_count;
    off_t start_offset = 0x40 - 8;
    int i;

	int Header =0;
	int HeaderSize = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mtaf",filename_extension(filename))) goto fail;

    /* check NPSF Header */
    if (read_32bitBE(0x00,streamFile) != 0x4D544146 ||
            read_32bitBE(0x40,streamFile) != 0x48454144)
        goto fail;

	do {
		start_offset +=HeaderSize + 8;
		Header = read_32bitBE(start_offset, streamFile);
		HeaderSize = read_32bitLE(start_offset + 4, streamFile);
	} while(Header!=0x44415441);

	start_offset +=4;

    /* check loop */
    loop_flag = 0;
    channel_count=2;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;

    /* Check for Compression Scheme */
    vgmstream->coding_type = coding_EACS_IMA;
    vgmstream->num_samples = read_32bitLE(start_offset,streamFile);
    vgmstream->layout_type = layout_mtaf_blocked;
    vgmstream->meta_type = meta_PS2_MTAF;
	vgmstream->block_count=1;
    start_offset += 4;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            if (!vgmstream->ch[0].streamfile) {
                vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,0x8000);
            }
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;

            if (!vgmstream->ch[i].streamfile) goto fail;

        }
    }
    
	mtaf_block_update(start_offset, vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
