#include "meta.h"
#include "../util.h"

/* Sony .ADS with SShd & SSbd Headers */

VGMSTREAM * init_vgmstream_ps2_ads(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int loop_flag=0;
    int channel_count;
    int i;

    /* check extension, case insensitive */
    if (strcasecmp("ads",filename_extension(filename)) && 
        strcasecmp("ss2",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check SShd Header */
    if (read_32bitBE(0x00,infile) != 0x53536864)
        goto fail;

    /* check SSbd Header */
    if (read_32bitBE(0x20,infile) != 0x53536264)
        goto fail;

    /* check if file is not corrupt */
    if (get_streamfile_size(infile)<(size_t)(read_32bitLE(0x24,infile) + 0x28))
        goto fail;

    /* check loop */
    loop_flag = (read_32bitLE(0x18,infile)!=0xFFFFFFFF);

    channel_count=read_32bitLE(0x10,infile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = read_32bitLE(0x10,infile);
    vgmstream->sample_rate = read_32bitLE(0x0C,infile);

    /* Check for Compression Scheme */
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitLE(0x24,infile)/16*28/vgmstream->channels;

    /* SS2 container with RAW Interleaved PCM */
    if (read_32bitLE(0x08,infile)==0x01) {
        vgmstream->coding_type=coding_PCM16LE;
        vgmstream->num_samples = read_32bitLE(0x24,infile)/2/vgmstream->channels;
    }

    /* Get loop point values */
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x18,infile);
        vgmstream->loop_end_sample = read_32bitLE(0x1C,infile);
    }

    /* don't know why, but it does happen, in ps2 too :( */
    if (vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->interleave_block_size = read_32bitLE(0x14,infile);
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_SShd;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,0x8000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                0x28+vgmstream->interleave_block_size*i;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
