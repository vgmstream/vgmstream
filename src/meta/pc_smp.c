#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* .smp file, with MS ADPCM. From Ghostbusters (PC). */

VGMSTREAM * init_vgmstream_pc_smp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    int channel_count;
    off_t start_offset;
    int interleave;

    int loop_flag = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("smp",filename_extension(filename))) goto fail;

    /* 6? */
    if (6 != read_32bitLE(0,streamFile)) goto fail;
    if (0 != read_32bitLE(0x14,streamFile)) goto fail;
    if (4 != read_32bitLE(0x24,streamFile)) goto fail;
    if (4 != read_32bitLE(0x2C,streamFile)) goto fail;

    start_offset = read_32bitLE(0x1c, streamFile);
    /* check body start + body size = total size */
    if (start_offset + read_32bitLE(0x20,streamFile) != get_streamfile_size(streamFile)) goto fail;

    /* might also be codec id? */
    channel_count = read_32bitLE(0x28,streamFile);
    if (channel_count != 1 && channel_count != 2) goto fail;

    /* verify MS ADPCM codec setup */
    {
        int i;
        /* coefficients and statement of 0x100 samples per block?? */
        static const uint8_t ms_setup[0x20] = {
        0x00,0x01,0x07,0x00,0x00,0x01,0x00,0x00,
        0x00,0x02,0x00,0xFF,0x00,0x00,0x00,0x00,
        0xC0,0x00,0x40,0x00,0xF0,0x00,0x00,0x00,
        0xCC,0x01,0x30,0xFF,0x88,0x01,0x18,0xFF};

        for (i = 0; i < 0x20; i++)
        {
            if ((uint8_t)read_8bit(0x34+i,streamFile) != ms_setup[i]) goto fail;
        }

        /* verify padding */
        for (i = 0x20; i+0x34 < start_offset; i++)
        {
            if (read_8bit(0x34+i,streamFile) != 0) goto fail;
        }
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitLE(0x18,streamFile);
    vgmstream->sample_rate = read_32bitLE(0x30,streamFile);

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;   // MS ADPCM does own interleave
    interleave = 0x86*channel_count;
    vgmstream->interleave_block_size = interleave;

    vgmstream->meta_type = meta_PC_SMP;

    /* open the file, set up each channel */
    {
        int i;

        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,
                channel_count*interleave*2);
        if (!vgmstream->ch[0].streamfile) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset =
                start_offset;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
