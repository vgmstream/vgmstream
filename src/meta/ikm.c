#include "meta.h"
#include "../coding/coding.h"


/* IKM - MiCROViSiON PS2 container [Zwei (PS2)] */
VGMSTREAM * init_vgmstream_ikm_ps2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"ikm") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x494B4D00) /* "IKM\0" */
        goto fail;

    if (read_32bitBE(0x40,streamFile) != 0x41535400) /* "AST\0" */
        goto fail;
    /* 0x20: type 03? */

    loop_flag = (read_32bitLE(0x14, streamFile) > 0);
    channel_count = read_32bitLE(0x50, streamFile);
    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IKM;
    vgmstream->sample_rate = read_32bitLE(0x44, streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x4c, streamFile), channel_count);
    vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile);
    vgmstream->loop_end_sample = read_32bitLE(0x18, streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10; /* @0x40 / channels */

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* IKM - MiCROViSiON PC container [Chaos Legion (PC)] */
VGMSTREAM * init_vgmstream_ikm_pc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;


    /* checks */
    if ( !check_extensions(streamFile,"ikm") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x494B4D00) /* "IKM\0" */
        goto fail;
    if (read_32bitBE(0x30,streamFile) != 0x4F676753) /* "OggS" */
        goto fail;
    /* 0x20: type 01? */

    start_offset = 0x30;
#ifdef VGM_USE_VORBIS
    {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_IKM;
        ovmi.loop_start     = read_32bitLE(0x14, streamFile);
        ovmi.loop_end       = read_32bitLE(0x18, streamFile);
        ovmi.loop_end_found = ovmi.loop_end;
        ovmi.loop_flag      = ovmi.loop_end > 0;
        ovmi.stream_size    = read_32bitLE(0x24, streamFile);

        vgmstream = init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, start_offset, &ovmi);
    }
#else
    goto fail;
#endif

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* IKM - MiCROViSiON PSP container [The Legend of Heroes: A Tear of Vermillion (PSP)] */
VGMSTREAM * init_vgmstream_ikm_psp(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset;
    size_t data_size;


    /* checks */
    if ( !check_extensions(streamFile,"ikm") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x494B4D00) /* "IKM\0" */
        goto fail;
    if (read_32bitBE(0x800,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    /* 0x20: type 00? */

    /* loop values (pre-adjusted without encoder delay) at 0x14/18 are found in the RIFF too */
    data_size = read_32bitLE(0x24, streamFile);
    start_offset = 0x800;

    temp_streamFile = setup_subfile_streamfile(streamFile, start_offset, data_size, "at3");
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_riff(temp_streamFile);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IKM;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
