#include "meta.h"
#include "../util.h"

/* NWA - Visual Arts streams
 *
 * This can apparently get a lot more complicated, I'm only handling the
 * raw PCM case at the moment (until I see something else).
 *
 * Kazunori "jagarl" Ueno's nwatowav was helpful, and will probably be used
 * to write coding support if it comes to that.
 */

VGMSTREAM * init_vgmstream_nwa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
	int i;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("nwa",filename_extension(filename))) goto fail;

    /* check that we're using raw pcm */
    if (
            read_32bitLE(0x08,streamFile)!=-1 || /* compression level */
            read_32bitLE(0x10,streamFile)!=0  || /* block count */
            read_32bitLE(0x18,streamFile)!=0  || /* compressed data size */
            read_32bitLE(0x20,streamFile)!=0  || /* block size */
            read_32bitLE(0x24,streamFile)!=0     /* restsize */
       ) goto fail;

    channel_count = read_16bitLE(0x00,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
    switch (read_16bitLE(0x02,streamFile)) {
        case 16:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->interleave_block_size = 2;
            break;
        case 8:
            vgmstream->coding_type = coding_PCM8;
            vgmstream->interleave_block_size = 1;
            break;
        default:
            goto fail;
    }
    vgmstream->num_samples = read_32bitLE(0x1c,streamFile)/channel_count;
    if (channel_count > 1) {
        vgmstream->layout_type = layout_interleave;
    } else {
        vgmstream->layout_type = layout_none;
    }
    vgmstream->meta_type = meta_NWA;

    /* open the file for reading by each channel */
    {
        STREAMFILE *chstreamfile;

        /* have both channels use the same buffer, as interleave is so small */
        chstreamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        
        if (!chstreamfile) goto fail;

        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = chstreamfile;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=0x2c+(off_t)(i*vgmstream->interleave_block_size);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
