#include "meta.h"
#include "../util.h"

/* P3D, with Radical ADPCM, from Prototype */

VGMSTREAM * init_vgmstream_p3d(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t parse_offset;
    off_t start_offset;
    size_t file_size;

    uint32_t header_size;
    uint32_t sample_rate;
    uint32_t body_bytes;
	int loop_flag;
	int channel_count;
    const int interleave = 0x14;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("p3d",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x503344FF) /* P3D\xFF */
		goto fail;
    header_size = read_32bitLE(0x4,streamFile);
    if (0xC != header_size) goto fail;
    file_size = get_streamfile_size(streamFile);
    if (read_32bitLE(0x8,streamFile) != file_size) goto fail;
    if (read_32bitBE(0xC,streamFile) != 0xFE) goto fail;
    /* body size twice? */
    if (read_32bitLE(0x10,streamFile) + header_size != file_size) goto fail;
    if (read_32bitLE(0x14,streamFile) + header_size != file_size) goto fail;

    /* mysterious 10! */
    if (read_32bitLE(0x18,streamFile) != 10) goto fail;

    /* parse header text */
    parse_offset = 0x1C;
    {
        int text_len = read_32bitLE(parse_offset,streamFile);
        if (9 != text_len) goto fail;
        /* AudioFile */
        if (read_32bitBE(parse_offset+4,streamFile) != 0x41756469 ||
            read_32bitBE(parse_offset+8,streamFile) != 0x6F46696C ||
            read_16bitBE(parse_offset+12,streamFile) != 0x6500) goto fail;
        parse_offset += 4 + text_len + 1;
    }
    {
        uint32_t name_count = read_32bitLE(parse_offset,streamFile);
        int i;
        parse_offset += 4;
        /* names? */
        for (i = 0; i < name_count; i++)
        {
            int text_len = read_32bitLE(parse_offset,streamFile);
            parse_offset += 4 + text_len + 1;
        }
    }
    /* info count? */
    if (1 != read_32bitLE(parse_offset,streamFile)) goto fail;
    parse_offset += 4;
    {
        int text_len = read_32bitLE(parse_offset,streamFile);
        if (4 != text_len) goto fail;
        /* radp */
        if (read_32bitBE(parse_offset+4,streamFile) != 0x72616470 ||
            read_8bit(parse_offset+8,streamFile) != 0) goto fail;
        parse_offset += 4 + text_len + 1;
    }

    /* real RADP header */
    if (0x52414450 != read_32bitBE(parse_offset,streamFile)) goto fail;
    channel_count = read_32bitLE(parse_offset+4,streamFile);
    sample_rate = read_32bitLE(parse_offset+8,streamFile);
    /* codec id? */
    //if (9 != read_32bitLE(parse_offset+0xC,streamFile)) goto fail;
    body_bytes = read_32bitLE(parse_offset+0x10,streamFile);
    start_offset = parse_offset+0x14;
    if (start_offset + body_bytes != file_size) goto fail;

    loop_flag = 0;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->coding_type = coding_RAD_IMA_mono;
    vgmstream->interleave_block_size = interleave;
    vgmstream->num_samples = body_bytes / interleave / channel_count * 32;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_P3D;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
   
            vgmstream->ch[i].offset=vgmstream->ch[i].channel_start_offset=start_offset+interleave*i;
        }
    }
    
	return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
