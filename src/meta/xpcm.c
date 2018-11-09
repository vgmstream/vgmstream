#include "meta.h"
#include "../coding/coding.h"

/* XPCM - from Circus games [Eternal Fantasy (PC), D.C. White Season (PC)] */
VGMSTREAM * init_vgmstream_xpcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t decompressed_size;
    int loop_flag, channel_count, codec, subcodec, sample_rate;


    /* checks */
    if (!check_extensions(streamFile, "pcm"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x5850434D) /* "XPCM" "*/
        goto fail;

    decompressed_size = read_32bitLE(0x04,streamFile); /* (data_size for PCM) */
    codec    = read_8bit(0x08, streamFile);
    subcodec = read_8bit(0x09, streamFile);
    /* 0x0a: always null */
    /* 0x0c: always 0x01 (PCM codec) */
    channel_count = read_16bitLE(0x0e,streamFile);
    sample_rate = read_32bitLE(0x10,streamFile);
    /* 0x14: average bitrate */
    /* 0x18: block size */
    /* 0x1a: output bits (16) */
    start_offset = 0x1c; /* compressed size in codec 0x01/03 */

    loop_flag  = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XPCM;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = decompressed_size / sizeof(int16_t) / channel_count;

    switch(codec) {
        case 0x00:
            if (subcodec != 0) goto fail;
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;
        case 0x02:
            if (subcodec != 0) goto fail;
            vgmstream->coding_type = coding_CIRCUS_ADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;
            break;

        case 0x01: /* LZSS + VQ */
        case 0x03: /* unknown */
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
