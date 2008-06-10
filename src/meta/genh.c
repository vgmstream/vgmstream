#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* GENH is an artificial "generic" header for headerless streams */

VGMSTREAM * init_vgmstream_genh(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
    
	int32_t channel_count;
    int32_t interleave;
    int32_t sample_rate;
    int32_t loop_start;
    int32_t loop_end;
    char filename[260];
    int coding;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("genh",filename_extension(filename))) goto fail;

    /* check header magic */
    if (read_32bitBE(0x0,streamFile) != 0x47454e48) goto fail;

    /* check format */
    /* 0 = PSX ADPCM */
    /* 1 = XBOX IMA ADPCM (36 byte blocks) */
    /* ... others to come */
    switch (read_32bitLE(0x18,streamFile)) {
        case 0:
            coding = coding_PSX;
            break;
        case 1:
            coding = coding_XBOX;
            break;
        default:
            goto fail;
    }

    channel_count = read_32bitLE(0x4,streamFile);
    interleave = read_32bitLE(0x8,streamFile);
    sample_rate = read_32bitLE(0xc,streamFile);
    loop_start = read_32bitLE(0x10,streamFile);
    loop_end = read_32bitLE(0x14,streamFile);

    if (coding == coding_XBOX && channel_count != 2) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,(loop_start!=-1));
    if (!vgmstream) goto fail;

    /* fill in the vital information */

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->loop_flag = (loop_start != -1);

	vgmstream->coding_type = coding;
    switch (coding) {
        case coding_PSX:
            vgmstream->interleave_block_size = interleave;
            if (channel_count > 1)
            {
                vgmstream->layout_type = layout_interleave;
            } else {
                vgmstream->layout_type = layout_none;
            }
            break;
        case coding_XBOX:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 36;

            break;
    }
    
	vgmstream->meta_type = meta_GENH;
    
    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            if (channel_count > 1 && vgmstream->layout_type == layout_interleave)
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,interleave);
            else
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=0x800;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
