#include "meta.h"
#include "../util.h"

/* STRM (from Final Fantasy Tactics A2 - Fuuketsu no Grimoire) */
VGMSTREAM * init_vgmstream_nds_strm_ffta2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

    int loop_flag;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("strm",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52494646 ||  /* RIFF */
        read_32bitBE(0x08,streamFile) != 0x494D4120)    /* "IMA " */
        goto fail;

    loop_flag = (read_32bitLE(0x20,streamFile) !=0);
    channel_count = read_32bitLE(0x24,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x2C;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->coding_type = coding_IMA_int;
    vgmstream->num_samples = (read_32bitLE(0x04,streamFile)-start_offset);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x20,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(0x28,streamFile);
    }

    vgmstream->interleave_block_size = 0x80;
    vgmstream->interleave_smallblock_size = (vgmstream->loop_end_sample)%((vgmstream->loop_end_sample)/vgmstream->interleave_block_size);
    vgmstream->layout_type = layout_interleave_shortblock;
    vgmstream->meta_type = meta_NDS_STRM_FFTA2;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
