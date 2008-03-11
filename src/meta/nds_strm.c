#include "nds_strm.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_nds_strm(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    coding_t coding_type;

    int codec_number;
    int channel_count;
    int loop_flag;

    off_t start_offset;

    /* check extension, case insensitive */
    if (strcasecmp("strm",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0,infile)!=0x5354524D ||
            (uint32_t)read_32bitBE(4,infile)!=0xFFFE0001)
        goto fail;

    /* check for HEAD section */
    if ((uint32_t)read_32bitBE(0x10,infile)!=0x48454144 || /* "HEAD" */
            (uint32_t)read_32bitLE(0x14,infile)!=0x50) /* 0x50-sized head is all I've seen */
        goto fail;

    /* check type details */
    codec_number = read_8bit(0x18,infile);
    loop_flag = read_8bit(0x19,infile);
    channel_count = read_8bit(0x1a,infile);

    switch (codec_number) {
        case 0:
            coding_type = coding_PCM8;
            break;
        case 1:
            coding_type = coding_PCM16LE;
            break;
        case 2:
            coding_type = coding_NDS_IMA;
            break;
        default:
            goto fail;
    }

    /* TODO: only mono and stereo supported */
    if (channel_count < 1 || channel_count > 2) goto fail;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitLE(0x24,infile);
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x1c,infile);
    /* channels and loop flag are set by allocate_vgmstream */
    vgmstream->loop_start_sample = read_32bitLE(0x20,infile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_type;
    vgmstream->meta_type = meta_STRM;

    vgmstream->interleave_block_size = read_32bitLE(0x30,infile);
    vgmstream->interleave_smallblock_size = read_32bitLE(0x38,infile);

    if (coding_type==coding_PCM8 || coding_type==coding_PCM16LE)
        vgmstream->layout_type = layout_none;
    else
        vgmstream->layout_type = layout_interleave_shortblock;

    start_offset = read_32bitLE(0x28,infile);

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            if (vgmstream->layout_type==layout_interleave_shortblock)
                vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,
                    vgmstream->interleave_block_size);
            else
                vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,
                    0x1000);
            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                start_offset + i*vgmstream->interleave_block_size;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
