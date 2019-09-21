#include "meta.h"
#include "../coding/coding.h"

/* ADPCM - from NAOMI/NAOMI2 Arcade games [F355 Challenge (Naomi)] */
VGMSTREAM * init_vgmstream_naomi_adpcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile, "adpcm"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41445043 ||  /* "ADPC" */
        read_32bitBE(0x04,streamFile) != 0x4D5F7630)    /* "M_v0" */
        goto fail;
    /* there is some kind of info in the end padding, loop related? */

    loop_flag = 0;
    channel_count = 2;
    start_offset = 0x40;
    data_size = read_32bitLE(0x10,streamFile) * 0x100; /* data has padding */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = yamaha_bytes_to_samples(data_size, channel_count);

    vgmstream->coding_type = coding_AICA_int;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = data_size / channel_count;
    vgmstream->meta_type = meta_NAOMI_ADPCM;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
