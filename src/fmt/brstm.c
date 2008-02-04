#include "brstm.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_brstm(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    coding_t coding_type;

    off_t head_offset;
    size_t head_length;
    off_t coef_offset;
    off_t coef_offset1;
    off_t coef_offset2;
    int codec_number;
    int channel_count;
    int loop_flag;

    off_t start_offset;

    /* check extension, case insensitive */
    if (strcasecmp("brstm",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0,infile)!=0x5253544D ||
            (uint32_t)read_32bitBE(4,infile)!=0xFEFF0100)
        goto fail;

    /* get head offset, check */
    head_offset = read_32bitBE(0x10,infile);
    if ((uint32_t)read_32bitBE(head_offset,infile)!=0x48454144)
        goto fail;
    head_length = read_32bitBE(0x14,infile);

    /* check type details */
    codec_number = read_8bit(head_offset+0x20,infile);
    loop_flag = read_8bit(head_offset+0x21,infile);
    channel_count = read_8bit(head_offset+0x22,infile);

    switch (codec_number) {
        case 0:
            coding_type = coding_PCM8;
            break;
        case 1:
            coding_type = coding_PCM16BE;
            break;
        case 2:
            coding_type = coding_NGC_DSP;
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
    vgmstream->num_samples = read_32bitBE(head_offset+0x2c,infile);
    vgmstream->sample_rate = read_16bitBE(head_offset+0x24,infile);
    /* channels and loop flag are set by allocate_vgmstream */
    vgmstream->loop_start_sample = read_32bitBE(head_offset+0x28,infile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = layout_interleave_shortblock;
    vgmstream->meta_type = meta_RSTM;

    vgmstream->interleave_block_size = read_32bitBE(head_offset+0x38,infile);
    vgmstream->interleave_smallblock_size = read_32bitBE(head_offset+0x48,infile);

    coef_offset1=read_32bitBE(head_offset+0x1c,infile);
    coef_offset2=read_32bitBE(head_offset+0x10+coef_offset1,infile);
    coef_offset=coef_offset2+0x10;

    {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i]=read_16bitBE(head_offset+coef_offset+i*2,infile);
        }

        if (vgmstream->channels==2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i]=read_16bitBE(head_offset+coef_offset+0x38+i*2,infile);
            }
        }
    }

    start_offset = read_32bitBE(head_offset+0x30,infile);

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,
                    vgmstream->interleave_block_size>0?
                    vgmstream->interleave_block_size:
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
