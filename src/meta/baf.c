#include "meta.h"
#include "../util.h"

/* .BAF - Bizarre Creations (Blur, James Bond 007: Blood Stone, etc) */

VGMSTREAM * init_vgmstream_baf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t WAVE_size,DATA_size;
    off_t start_offset;
    long sample_count;

    const int frame_size = 33;
    const int frame_samples = 64;
    int channels;
    int loop_flag = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("baf",filename_extension(filename))) goto fail;

    /* check WAVE */
    if (read_32bitBE(0,streamFile) != 0x57415645) goto fail;
    WAVE_size = read_32bitBE(4,streamFile);
    if (WAVE_size != 0x4c) goto fail;
    /* check for DATA after WAVE */
    if (read_32bitBE(WAVE_size,streamFile) != 0x44415441) goto fail;
    /* check that WAVE size is data size */
    DATA_size = read_32bitBE(0x30,streamFile);
    if (read_32bitBE(WAVE_size+4,streamFile)-8 != DATA_size) goto fail;

    sample_count = read_32bitBE(0x44,streamFile);

    /* unsure how to detect channel count, so use a hack */
    channels = (long long)DATA_size / frame_size * frame_samples / sample_count;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = WAVE_size + 8;
    vgmstream->sample_rate = read_32bitBE(0x40,streamFile);
    vgmstream->num_samples = sample_count;

    vgmstream->coding_type = coding_BAF_ADPCM;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;
    vgmstream->meta_type = meta_BAF;

    /* open the file for reading by each channel */
    {
        int i;
        STREAMFILE *file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channels;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

