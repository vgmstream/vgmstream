#include "meta.h"

/* .BAF - Bizarre Creations (Blur, James Bond 007: Blood Stone, etc) */
VGMSTREAM * init_vgmstream_baf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t WAVE_size, DATA_size;
    off_t start_offset;
    long sample_count;
    int sample_rate;

    const int frame_size = 33;
    const int frame_samples = (frame_size-1) * 2;
    int channels;
    int loop_flag = 0;

    /* check extensions */
    if ( !check_extensions(streamFile, "baf") )
        goto fail;

    /* check WAVE */
    if (read_32bitBE(0,streamFile) != 0x57415645) /* "WAVE" */
        goto fail;
    WAVE_size = read_32bitBE(4,streamFile);
    if (WAVE_size != 0x4c) /* && WAVE_size != 0x50*/
        goto fail;
    /* check for DATA after WAVE */
    if (read_32bitBE(WAVE_size,streamFile) != 0x44415441) /* "DATA"*/
        goto fail;
    /* check that WAVE size is data size */
    DATA_size = read_32bitBE(0x30,streamFile);
    if (read_32bitBE(WAVE_size+4,streamFile)-8 != DATA_size) goto fail;

    /*if (WAVE_size == 0x50) sample_count = DATA_size * frame_samples / frame_size / channels;*/
    sample_count = read_32bitBE(0x44,streamFile);

    /*if (WAVE_size == 0x50) sample_rate = read_32bitBE(0x3c,streamFile);*/
    sample_rate = read_32bitBE(0x40,streamFile);

    /* unsure how to detect channel count, so use a hack */
    channels = (long long)DATA_size / frame_size * frame_samples / sample_count;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = WAVE_size + 8;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;

    vgmstream->coding_type = coding_PSX_cfg;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;
    vgmstream->meta_type = meta_BAF;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

