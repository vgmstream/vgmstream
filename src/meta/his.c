#include "meta.h"
#include "../util.h"

/* Her Interactive Sound .his (Nancy Drew) */

VGMSTREAM * init_vgmstream_his(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int channel_count;
    int loop_flag = 0;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("his",filename_extension(filename))) goto fail;

    /* check header magic */
    const uint8_t header_magic_expected[0x16] = "Her Interactive Sound\x1a";
    uint8_t header_magic[0x16];
    if (0x16 != streamFile->read(streamFile, header_magic, 0, 0x16)) goto fail;

    if (memcmp(header_magic_expected, header_magic, 0x16)) goto fail;

    /* data chunk label */
    if (0x64617461 != read_32bitBE(0x24,streamFile)) goto fail;

    start_offset = 0x2c;

    /* channel count is there twice, or one is bytes per frame */
    channel_count = read_16bitLE(0x16,streamFile);
    if (read_16bitLE(0x20,streamFile) != channel_count) goto fail;

    /* 8-bit expected */
    if (read_16bitLE(0x22,streamFile) != 8) goto fail;

    /* check size */
    /* file size -8, I assume taken from RIFF, though it doesn't describe this format well */
	if ((read_32bitLE(0x1c,streamFile)+8) != get_streamfile_size(streamFile))
		goto fail;
    /* data chunk size, assume it occupies the rest of the file */
    if ((read_32bitLE(0x28,streamFile)+start_offset) != get_streamfile_size(streamFile))
        goto fail;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->num_samples = read_32bitLE(0x28,streamFile) / channel_count;
    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);

	vgmstream->coding_type = coding_PCM8_U;
    vgmstream->meta_type = meta_HIS;
    vgmstream->layout_type = layout_none;
    if (channel_count == 2)
    {
        vgmstream->coding_type = coding_PCM8_U_int;
        vgmstream->interleave_block_size = 1;
    }

    /* open the file for reading */
    {
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        vgmstream->ch[0].streamfile = file;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;

        if (channel_count == 2)
        {
            file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!file) goto fail;
            vgmstream->ch[1].streamfile = file;
        
            vgmstream->ch[0].channel_start_offset=
                vgmstream->ch[1].offset=start_offset + vgmstream->interleave_block_size;
        }
    }
    
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
