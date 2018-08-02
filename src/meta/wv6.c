#include "meta.h"
#include "../coding/coding.h"

/* WV6 - Gorilla Systems PC games [Spy Kids: Mega Mission Zone (PC), Lilo & Stitch: Hawaiian Adventure (PC)] */
VGMSTREAM * init_vgmstream_wv6(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "wv6"))
        goto fail;

    if (read_32bitLE(0x00,streamFile) != get_streamfile_size(streamFile))
        goto fail;
    if (read_32bitBE(0x2c,streamFile) != 0x57563620 ||  /* "WV6 " */
        read_32bitBE(0x30,streamFile) != 0x494D415F)    /* "IMA_" ("WV6 IMA_ADPCM COMPRESSED 16 BIT AUDIO") */
        goto fail;

    /* 0x54/58/5c/60/6c: unknown (reject to catch possible stereo files, but don't seem to exist) */
    if (read_32bitLE(0x54,streamFile) != 0x01 ||
        read_32bitLE(0x58,streamFile) != 0x01 ||
        read_32bitLE(0x5c,streamFile) != 0x10 ||
        read_32bitLE(0x68,streamFile) != 0x01 ||
        read_32bitLE(0x6c,streamFile) != 0x88)
        goto fail;
    /* 0x64: PCM size (samples*channels*2) */

    channel_count = 1;
    loop_flag = 0;
    start_offset = 0x8c;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x60, streamFile);
    vgmstream->num_samples = ima_bytes_to_samples(read_32bitLE(0x88,streamFile), channel_count);

    vgmstream->meta_type = meta_WV6;
    vgmstream->coding_type = coding_WV6_IMA;
    vgmstream->layout_type = layout_none;

    read_string(vgmstream->stream_name,0x1c+1, 0x04,streamFile);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
