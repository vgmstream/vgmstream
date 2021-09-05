#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* .ADX - from Xenoblade 3D (3DS) */
VGMSTREAM* init_vgmstream_dsp_adx(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channels;
    int channel_header_spacing = 0x34;

    /* checks */
    if (!check_extensions(sf,"adx"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x02000000)
        goto fail;

    channels = read_32bitLE(0, sf);
    loop_flag = read_16bitLE(0x6e, sf);

    if (channels > 2 || channels < 0) goto fail;

    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_XB3D_ADX;
    vgmstream->sample_rate = read_32bitLE(0x70,sf);
    vgmstream->num_samples = read_32bitLE(0x74, sf);
    vgmstream->loop_start_sample = read_32bitLE(0x78, sf);
    vgmstream->loop_end_sample = read_32bitLE(0x7c, sf);

    dsp_read_coefs_le(vgmstream,sf, 0x4, channel_header_spacing);

    /* semi-interleave: manually open streams at offset */
    {
        char filename[PATH_LIMIT];
        int i;

        sf->get_name(sf,filename,sizeof(filename));
        for (i = 0; i<channels; i++) {
            vgmstream->ch[i].streamfile = sf->open(sf, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
            vgmstream->ch[i].channel_start_offset =
                    vgmstream->ch[i].offset = read_32bitLE(0x34+i*channel_header_spacing, sf);
            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

#if 0
    /* this should be equivalent to the above, but more testing is needed */
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size =
            read_32bitLE(0x34+1*channel_header_spacing, streamFile)
            - read_32bitLE(0x34+0*channel_header_spacing, streamFile);

    if (!vgmstream_open_stream(vgmstream,streamFile, read_32bitLE(0x34+0*channel_header_spacing, streamFile)))
        goto fail;
#endif

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
