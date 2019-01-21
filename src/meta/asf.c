#include "meta.h"
#include "../coding/coding.h"

/* ASF - Argonaut PC games [Croc 2 (PC), Aladdin: Nasira's Revenge (PC)] */
VGMSTREAM * init_vgmstream_asf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, version;


    /* checks */
    /* .asf: original
     * .lasf: fake for plugins */
    if (!check_extensions(streamFile, "asf,lasf"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x41534600) /* "ASF\0" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x02000100)
        goto fail;
    if (read_32bitLE(0x08,streamFile) != 0x01 &&
        read_32bitLE(0x0c,streamFile) != 0x18 &&
        read_32bitLE(0x1c,streamFile) != 0x20)
        goto fail;

    version = read_32bitLE(0x28,streamFile); /* assumed? */
    switch(version){
        case 0x0d: channel_count = 1; break; /* Aladdin: Nasira's Revenge (PC) */
        case 0x0f: channel_count = 2; break; /* Croc 2 (PC), The Emperor's New Groove (PC) */
        default: goto fail;
    }

    loop_flag = 0;
    start_offset = 0x2c;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x24, streamFile);
    vgmstream->meta_type = meta_ASF;
    vgmstream->coding_type = coding_ASF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x11;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/(0x11*channel_count)*32; /* bytes_to_samples */
    //vgmstream->num_samples = read_32bitLE(0x18,streamFile) * (0x20<<channel_count); /* something like this? */

    read_string(vgmstream->stream_name,0x10, 0x08+1,streamFile);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
