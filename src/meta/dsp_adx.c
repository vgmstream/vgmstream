#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* .ADX - from Xenoblade 3D */
/* Xenoblade Chronicles 3D uses an adx extension as with
 * the Wii version, but it's actually DSP ADPCM. */
VGMSTREAM * init_vgmstream_dsp_adx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    int channel_header_spacing = 0x34;
	
    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"adx")) goto fail;

    /* check header */
    if (read_32bitBE(0,streamFile)!=0x02000000) goto fail;

    channel_count = read_32bitLE(0, streamFile);
    loop_flag = read_16bitLE(0x6e, streamFile);

    if (channel_count > 2 || channel_count < 0) goto fail;

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_XB3D_ADX;
    vgmstream->sample_rate = read_32bitLE(0x70,streamFile);
    vgmstream->num_samples = read_32bitLE(0x74, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x78, streamFile);
    vgmstream->loop_end_sample = read_32bitLE(0x7c, streamFile);

    dsp_read_coefs_le(vgmstream,streamFile, 0x4, channel_header_spacing);


    /* semi-interleave: manually open streams at offset */
    {
        char filename[PATH_LIMIT];
        int i;

        streamFile->get_name(streamFile,filename,sizeof(filename));
        for (i = 0; i<channel_count; i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
            vgmstream->ch[i].channel_start_offset =
                    vgmstream->ch[i].offset = read_32bitLE(0x34+i*channel_header_spacing, streamFile);
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
